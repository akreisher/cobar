#include <errno.h>
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

/* Flags shared by monitors/desktops
   Not all may be applicable */
enum flags {
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
  int n_desktops;
  struct desktop_info dts[MAX_DESKTOPS];
};

static void generate_desktop_output(block_text *texts, struct monitor_info *monitors) {
  /* Generate desktop output based on the current monitors value. */
  int i, j, num;
  block_text *text;
  char buf[TEXT_LEN];
  char *out;
  
  for (i = 0; i < MAX_MONITORS; i++) {
    for (j = 0; j < MAX_DESKTOPS; j++) {

	  text = &texts[i * MAX_DESKTOPS + j];

	  if (!monitors[i].dts[j].flags) {
        text->text[0] = '\0';
        continue;
      }

	  text->monitor = i;
	  
	  /* Command */
	  
	  snprintf(buf, TEXT_LEN, "desktop %s", monitors[i].dts[j].name);
	  SET_COMMAND(text[0], buf);

	  /* Color */

	  if (monitors[i].dts[j].flags & URGENT) {
		SET_COLOR(text[0], 0xFF0000);
	  }
	
	  else if (monitors[i].dts[j].flags & FOCUSED) {
		if (monitors[i].flags & MONOCLE)
		  SET_COLOR(text[0], 0xFFFF00);
		else
		  SET_COLOR(text[0], 0xFFFFFF);
	  }
	
	  else {
		SET_COLOR(text[0], 0x808080);
	  }

	  /* Text */
	  out = text->text;
	  *(out++) = ' ';
	  
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
    
	  *(out++) = '\0';

	  log_debug("Output: %s", text->text);
	}
  }
}

static inline void monitor_report(struct monitor_info *m, char *r) {
  int i = 0;

  switch (*r++) {
  case 'M':
    m->flags = FOCUSED;
    break;
  case 'm':
    m->flags = 0;
    break;
  default:
    break;
  }

  while (*r != ':' && *r != '\0') {
    if (i >= MAX_NAME_LEN) {
      log_error("Monitor name too long");
	  exit(EXIT_FAILURE);
    }
	m->name[i++] = *(r++);
  }
  m->name[i] = '\0';
  log_debug("MONITOR NAME: %s", m->name);
}

static inline void desktop_report(struct desktop_info *d, char *r) {
  int i = 0;

  switch (*r++) {
  case 'f':
    d->flags = 0;
    break;
  case 'F':
    d->flags = FOCUSED;
    break;
  case 'o':
    d->flags = OCCUPIED;
    break;
  case 'O':
    d->flags = FOCUSED | OCCUPIED;
    break;
  case 'u':
    d->flags = URGENT | OCCUPIED;
    break;
  case 'U':
    d->flags = FOCUSED | URGENT | OCCUPIED;
    break;
  default:
	break;
  }
  
  while (*r != ':' && *r != '\0') {
    if (i >= MAX_NAME_LEN) {
      log_error("Desktop name too long");
	  exit(EXIT_FAILURE);
    }
	d->name[i++] = *(r++);
  }
  d->name[i] = '\0';
  log_debug("DESKTOP NAME: %s", d->name);
}

void desktop_event(FILE *bspc_fd, struct monitor_info *monitors) {
  static char event[256], name[MAX_NAME_LEN], command[64];
  char *ptr;
  int i, j, focused;
  FILE *query;

  fgets(event, 256, bspc_fd);
  event[strcspn(event, "\r\n")] = '\0';
  log_debug("bspc report: %s", event);

  ptr = event;
  i = -1; // Monitor index, start at -1 to get to 0 on first.
  j = 0; // Desktop index
  
  while (ptr++) {
	switch (*ptr) {
	/* New Monitor */
    case 'M': case 'm': {
	  monitor_report(&monitors[++i], ptr);
	  j = 0;
	  break;
	}

	/* New desktop */
    case 'o': case 'O': case 'f': case 'F': case 'u': case 'U':	  {
	  desktop_report(&monitors[i].dts[j++], ptr);
	  break;
	}

	/* Monitor layout */
    case 'L': {
	  if (ptr[1] == 'M') monitors[i].flags |= MONOCLE;
	  break;
	}

    default:
	  break;
    }
    /* Jump to next section */
    ptr = strpbrk(ptr+1, ":");
  }
}

void *desktop_block (void *input) {
  FILE *bspc_fd;
  struct monitor_info monitors[MAX_MONITORS];
  
  block_internal block;
  init_internal(input, &block);

  bspc_fd = popen("bspc subscribe report", "r");
  while (1) {

	desktop_event(bspc_fd, monitors);

	generate_desktop_output(block.text, monitors);
	
	write_data(&block);
    
  }

  pclose(bspc_fd);
}
