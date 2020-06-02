#include <fcntl.h>
#include <libgen.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log/log.h"
#include "modules.h"

typedef unsigned long int uli;

#define MAX_NAME_LEN 16

#define GET_ID_NAME(st, fd_id, fd_names) \
  (fscanf(fd_id, "%lx", &st.id) \
   && fgets(st.name, MAX_NAME_LEN, fd_names))

enum flags {
  FREE     = 0,
  FOCUSED  = 1 << 0,
  OCCUPIED = 1 << 2,
  URGENT   = 1 << 3,
  MONOCLE  = 1 << 4,
};

struct desktop_info {
  char name[MAX_NAME_LEN];
  uli id;
  int flags;
};

struct monitor_info {
  char name[MAX_NAME_LEN];
  uli id;
  int flags;
  struct desktop_info dts[NUM_DESKTOPS];
};

static struct monitor_info monitors[NUM_MONITORS];

static void get_wm_state() {
  /* Fill in the monitors array with current BSPWM data. */
  int i, j;
  FILE *fd_id, *fd_names;
  char cmd[64];

  fd_id = popen("bspc query -M", "r");
  fd_names = popen("bspc query --names -M", "r");
  log_debug("Getting BSPWM state");
  for (i = 0; i < NUM_MONITORS; i++) {
    if (GET_ID_NAME(monitors[i], fd_id, fd_names)) {
      monitors[i].name[strcspn(monitors[i].name, "\r\n")] = '\0';
      log_debug("Monitor: %s", monitors[i].name);		
    }
    else {
      /* Monitor does not exist */
      monitors[i].id = 0;
      log_debug("Monitor: NULL");
    }
  }
  pclose(fd_id);
  pclose(fd_names);

  for (i = 0; i < NUM_MONITORS && monitors[i].id; i++) {
    sprintf(cmd, "bspc query -D -m %s", monitors[i].name);
    fd_id = popen(cmd, "r");
    sprintf(cmd, "bspc query -D -m %s --names", monitors[i].name);
    fd_names = popen(cmd, "r");
    for (j = 0; j < NUM_DESKTOPS; j++) {
      if (GET_ID_NAME(monitors[i].dts[j], fd_id, fd_names)) {
	monitors[i].dts[j].name[strcspn(monitors[i].dts[j].name, "\r\n")] = '\0';
	log_debug("Desktop: %s", monitors[i].dts[j].name);
      }
      else {
	/* Desktop does not exist */
	monitors[i].dts[j].id = 0;
	log_debug("Desktop: NULL");
      }
    }
    pclose(fd_id);
    pclose(fd_names);
  }
}

static void get_desktop_output(char *out) {
  /* Generate desktop output based on the current monitors value. */
  int i, j, num;
  for (i = 0; i < NUM_MONITORS && monitors[i].id; i++) {
    out += sprintf(out, "%%{S%d}", i);
    for (j = 0; j < NUM_DESKTOPS && monitors[i].dts[j].id; j++) {
      if (monitors[i].dts[j].flags) {

	/* Add clickable command */
	out += sprintf(out, "%%{A:desktop %lX:}", monitors[i].dts[j].id);

	if (monitors[i].dts[j].flags & URGENT) {
	  out += sprintf(out, "%%{F#FF0000}");
	}
	
	else if (monitors[i].dts[j].flags & FOCUSED) {
	  if (monitors[i].flags & MONOCLE)
	    out += sprintf(out, "%%{F#00FFFF}");
	  else
	    out += sprintf(out, "%%{F#FFFFFF}");
	}
	
	else {
	  out += sprintf(out, "%%{F#808080}");
	}
	
	if (monitors[i].dts[j].flags & FOCUSED) {
	  *(out++) = '[';
	}

	out += sprintf(out, "%d", j+1);
	if (sscanf(monitors[i].dts[j].name, "%d", &num) != 1 || num != j + 1) {
	  out += sprintf(out, ":%s", monitors[i].dts[j].name);
	}

	if (monitors[i].dts[j].flags & FOCUSED) {
	  *(out++) =']';
	}
    
	*(out++) = ' ';
	out += sprintf(out, "%%{A}");
      }
    }
  }
}

void desktop_event(FILE *bspc_fd) {
  static char event[256], name[16], *ptr;
  int i, j;

  fgets(event, 256, bspc_fd);
  event[strcspn(event, "\r\n")] = '\0';
  log_debug("bspc report: %s", event);

  ptr = event;
  i = -1; // Monitor index, start at -1 to get to 0 on first.
  j = 0; // Desktop index
  
  while (ptr) {
    /* Check first char of new section */
    switch (ptr[1]) {

    /* New Monitor */
    case 'M': {
      monitors[++i].flags = FOCUSED;
      j = 0;
      break;
    }
    case 'm': {
      monitors[++i].flags = 0;
      j = 0;
      break;
    }

    /* New desktop */
    case 'o': {
      monitors[i].dts[j++].flags = OCCUPIED;
      break;
    }
    case 'O': {
      monitors[i].dts[j++].flags = OCCUPIED | FOCUSED;
      break;
    }
    case 'f': {
      monitors[i].dts[j++].flags = FREE;
      break;
    }
    case 'F': {
      monitors[i].dts[j++].flags = FOCUSED;
      break;
    }
    case 'U': {
      monitors[i].dts[j++].flags = URGENT | FOCUSED;
      break;
    }
    case 'u': {
      monitors[i].dts[j++].flags = URGENT;
      break;
    }

    /* WM state */
    case 'L': {
      if (ptr[2] == 'M') monitors[i].flags |= MONOCLE;
    }
    default:
      break;
    }
    /* Jump to next section */
    ptr = strpbrk(ptr+1, ":");
  }
}

void *desktop_block (void *input) {
  char buf[512];
  FILE *bspc_fd;

  block_input *in;
  block_output out;
    
    
  in = (block_input *) input;
  init_output(in, &out);

  get_wm_state();

  bspc_fd = popen("bspc subscribe report", "r");
  while (1) {
    get_desktop_output(buf);
    snprintf(out.data, 400, "%%{F#808080} %s %%{F-}%%{B-}", buf);
    write_data(&out);
    // Wait for desktop focus change
    desktop_event(bspc_fd);
  }

  pclose(bspc_fd);
}
