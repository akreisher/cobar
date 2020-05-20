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

enum desktop_flags {
  FOCUSED = 1,
  OCCUPIED = 1 << 2,
};

struct desktop_info {
  unsigned long int id;
  int flags;
  char name[16];
};

static void get_desktop_info(struct desktop_info *dts, int nd) {

  int i = 0;
  FILE *fd_id, *fd_names;

  fd_id = popen("bspc query -D", "r");
  fd_names = popen("bspc query --names -D", "r");

  for (i = 0; i < nd; i++) {
  
    if (fscanf(fd_id, "%lx", &dts[i].id) != 1)
      break;
    if (!fgets(dts[i].name, 16, fd_names))
      break;
    dts[i].name[strcspn(dts[i].name, "\r\n")] = '\0';
    log_debug("%s", dts[i].name);
  }

  pclose(fd_id);
  pclose(fd_names);

}

static void get_desktop_output(const struct desktop_info * dts,
			       int nd,
			       char *out) {
  int i, num, offset = 0;
  for (i = 0; i < nd; i++) {
    if (dts[i].flags & (FOCUSED | OCCUPIED)) {
      offset += sprintf(out + offset, "%%{A:desktop %lX:}", dts[i].id);
      if (dts[i].flags & FOCUSED) {
	offset += sprintf(out + offset, "%%{F#FFFFFF}");
	out[offset++] = '[';
      }

      offset += sprintf(out + offset, "%d", i+1);

      if (sscanf(dts[i].name, "%d", &num) != 1 || num != i + 1) {
        offset += sprintf(out + offset, ":%s", dts[i].name);
      }

      if (dts[i].flags & FOCUSED) {
	out[offset++] =']';
	offset += sprintf(out + offset, "%%{F#808080}");
      }
    
      out[offset++] = ' ';
      offset += sprintf(out + offset, "%%{A}");
    }
  }
}

void desktop_event(FILE *bspc_fd, struct desktop_info *dts) {
  static char event[128], name[16], *ptr;
  static unsigned long int monitor_id, desktop_id;
  int i = 0;

  fgets(event, 128, bspc_fd);
  ptr = event;

  printf("Here!\n");
  log_debug("bspc report: %s", event);
  
  while (ptr) {
    switch (ptr[1]) {
    case 'o': {
      dts[i++].flags = OCCUPIED;
      break;
    }

    case 'O': {
      dts[i++].flags = OCCUPIED | FOCUSED;
      break;
    }

    case 'F': {
      dts[i++].flags = FOCUSED;
      break;
    }

    case 'f': {
      dts[i++].flags = 0;
      break;
    }

    default:
      break;
    }
    ptr = strpbrk(ptr+1, ":");
  }
}

void *desktop_block (void *input) {
  log_info("de");
  char buf[512];
  FILE *bspc_fd;
  unsigned long int monitor_id, desktop_id;

  block_input *in;
  block_output out;
    
    
  in = (block_input *) input;
  init_output(in, &out);

  struct desktop_info desktops[desktop_args.num_desktops];

  get_desktop_info(desktops, desktop_args.num_desktops);

  // Initial desktop
  bspc_fd = popen("bspc query -D -d", "r");
  fscanf(bspc_fd, "%lx", &desktop_id);
  pclose(bspc_fd);

  bspc_fd = popen("bspc subscribe report", "r");
  while (1) {
    get_desktop_output(desktops, desktop_args.num_desktops, buf);
    snprintf(out.data, 400, "%%{F#808080} %s %%{F-}%%{B-}", buf);
    write_data(&out);
    // Wait for desktop focus change
    desktop_event(bspc_fd, desktops);
  }

  pclose(bspc_fd);
}
