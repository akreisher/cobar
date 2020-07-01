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

#include "modules.h"


static const char *get_label(enum blocks id);
static const char *get_command(enum blocks id);

void
init_text(block_text *text) {
  memset(text, 0, sizeof(block_text));
  text->color = 0xFFFFFF;
  text->monitor = -1;  // Default to all monitors.
}

void
init_internal(const void *input, block_internal *internal) {
  const block_input *in = (block_input *) input;
  internal->id = in->id;
  internal->pipes[0] = in->pipes[0];
  internal->pipes[1] = in->pipes[1];
  internal->nt = in->nt;
  internal->text = malloc(in->nt * sizeof(block_text));
  for (int i = 0; i < in->nt; i++) {
    init_text(&internal->text[i]);
	SET_LABEL(internal->text[i], get_label(internal->id));
	SET_COMMAND(internal->text[i], get_command(internal->id));
  }
}

void
free_internal(block_internal *internal) { free(internal->text); }

inline void
write_data(const block_internal *internal) {
  write(internal->pipes[1], &internal->id, sizeof(enum blocks));
  for (int i = 0; i < internal->nt; i++)
    write(internal->pipes[1], &internal->text[i], sizeof(block_text));
}


void *battery_block(void *input) {
  int cap;
  FILE *f;

  block_internal block;
  init_internal(input, &block);

  f = fopen("/sys/class/power_supply/BAT0/capacity" , "r");

  while (1) {
    fscanf(f, "%d", &cap);
  
    if      (cap <= battery_args.bat_crit) SET_COLOR(block.text[0], 0xFF0000);
    else if (cap <= battery_args.bat_warn) SET_COLOR(block.text[0], 0xFFFC00);
    else                                   SET_COLOR(block.text[0], 0xFFFFFF);

    snprintf(block.text[0].text, TEXT_LEN, "%d", cap);
    write_data(&block);

    sleep(battery_args.dt);
  }
  free_internal(&block);
  fclose(f);
}


void *clock_block (void *input) {
  time_t t;

  block_internal block;
  block_input *in = (block_input *) input;
  init_internal(in, &block);

  while (1) {
    t = time(NULL);
    strftime(block.text[0].text, 32, clock_args.time_format, localtime(&t));
    write_data(&block);
    sleep(clock_args.dt);
  }
}

/* CPU Usage */
void *cpu_block (void *input) {

  char *color;
  FILE *cpuinfo;
  unsigned long long int user, nice, sys, idle, iowait, irq, sirq, \
    steal, guest, nguest, in_use, total, old_in_use, old_total;
  float percent;

  block_internal block;
  init_internal(input, &block);

  cpuinfo = fopen("/proc/stat", "r");

  if (fscanf(cpuinfo,
	     "cpu  %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
	     &user, &nice, &sys, &idle, &iowait, &irq, &sirq,
	     &steal, &guest, &nguest) != 10) {
    perror("Couldn't read CPU usage from /proc/stat\n");
    exit(1);
  }

  fclose(cpuinfo);

  old_in_use = user + nice + sys + irq + sirq + steal + guest + nguest;
  old_total = in_use + idle + iowait;

  while (1) {

    cpuinfo = fopen("/proc/stat", "r");

    if (fscanf(cpuinfo,
	       "cpu  %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
	       &user, &nice, &sys, &idle, &iowait, &irq, &sirq,
	       &steal, &guest, &nguest) != 10) {
      perror("Couldn't read CPU usage from /proc/stat\n");
      exit(1);
    }

    fclose(cpuinfo);

    in_use = user + nice + sys + irq + sirq + steal + guest + nguest;
    total = in_use + idle + iowait;

    percent = 100.0f * (in_use - old_in_use) / (float) (total - old_total);

    // Get color
    if (percent > cpu_args.cpu_crit)       SET_COLOR(block.text[0], 0xFF0000);
    else if (percent > cpu_args.cpu_warn)  SET_COLOR(block.text[0], 0xFFFC00);
    else                                   SET_COLOR(block.text[0], 0xFFFFFF);

    snprintf(block.text[0].text, 32, "%.1lf%%", percent);
    write_data(&block);

    old_in_use = in_use;
    old_total = total;

    sleep(cpu_args.dt);
  }
}


void *mail_block (void *input) {

  FILE *mail_fd;
  int unread;

  block_internal block;
  init_internal(input, &block);

  while (1) {
    mail_fd = popen(mail_args.command, "r");
    fscanf(mail_fd, "%d\n", &unread);
    pclose(mail_fd);

    sprintf(block.text[0].text, "%d", unread);
    write_data(&block);
    usleep(2000);
    read(block.pipes[0], block.text[0].text, 1);
  }

  
}

void *mem_block (void *input) {

  FILE *meminfo_fd;
  int mem_free, mem_tot;

  block_internal block;
  init_internal(input, &block);

  while (1) {

    meminfo_fd = fopen("/proc/meminfo", "r");
    fscanf(meminfo_fd, "MemTotal: %d kB\nMemFree: %d kB", &mem_tot, &mem_free);
    fclose(meminfo_fd);

    snprintf(block.text[0].text, 32, "%.1f G",
	    ((float) mem_free) / ((float) (1<<20)));
    write_data(&block);
    sleep(mem_args.dt);
  }
}


/* Temperature */
void *temp_block (void *input) {

  char *color;
  char command[64] = "sensors -u ";
  FILE *sensors_fd;
  float temp;

  block_internal block;
  init_internal(input, &block);

  strncat(command, temp_args.chip, 48);

  while (1) {

    sensors_fd = popen(command, "r");

    while (fgets(block.text[0].text, TEXT_LEN, sensors_fd)) {
      if (sscanf(block.text[0].text, "  temp1_input: %f", &temp))
	break;
    }
    pclose(sensors_fd);

    if (temp > temp_args.T_crit)
      SET_COLOR(block.text[0], 0xFF0000);
    else if (temp > temp_args.T_warn)
      SET_COLOR(block.text[0], 0xFFFC00);
    else
      SET_COLOR(block.text[0], 0xFFFFFF);

    snprintf(block.text[0].text, TEXT_LEN, "%.1f°C", temp);
    write_data(&block);
    sleep(temp_args.dt);
  }
}

void *vol_block(void *input) {

  FILE *pulse_fd;
  int vol[2];

  block_internal block;
  init_internal(input, &block);

  while (1) {
    pulse_fd = popen("pulsemixer --get-volume", "r");
    fscanf(pulse_fd, "%d %d\n", vol, vol + 1);
    pclose(pulse_fd);

    snprintf(block.text[0].text, TEXT_LEN, "%d", vol[0]);
    write_data(&block);
    usleep(2000);
    read(block.pipes[0], block.text[0].text, 1);
  }
}


/* void *systray(void *input) { */
/*   char buf[256], desktop[32]; */
/*   FILE *systray, *xdo; */
/*   int fifo, width, height, pos_x, pos_y; */
/*   unsigned long systray_wid; */
/*   float temp; */

/*   tray_arg* arg; */
/*   block_input* in; */
/*   block_output out; */

/*   in = (block_input *) input; */
/*   arg = (tray_arg *) in->arg; */
/*   out.id = in->id; */
/*   out.data = buf; */


/*   snprintf(buf, 256, */
/*            "stalonetray --geometry 1x1+%d-%d -i %d \ */
/*            --grow-direction W --log-level info \ */
/*            --kludges fix_window_pos,force_icons_size,use_icons_hints", */
/* 	   arg->x_pos, arg->y_pos, arg->icon_size); */

/*   systray = popen(buf, "r"); */

/*   // Get id */
/*   xdo = popen("xdo id -a cobar", "r"); */
/*   fscanf(xdo, "%lx", &systray_wid); */
/*   pclose(xdo); */

/*   while (1) { */

/*     // read input from tray */
/*     fgets(buf, 128, systray); */
/*     printf("%s\n", buf); */
/*     if ( sscanf(buf, "geometry: %dx%d+%d+%d", */
/* 		&width, &height, &pos_x, &pos_y) == 4 ) { */
/*       // Write new geometry to bar */
/*       out.flags = BAR_RESIZE; */
/*       write(fifo, &out, sizeof(out)); */
/*     } */
/*   } */
/* } */


void *(*get_block_func (enum blocks id)) (void *) {
  switch (id) {
  case BATTERY:
    return battery_block;
  case CLOCK:
    return clock_block;
  case CPU:
    return cpu_block;
  case DESKTOP:
    return desktop_block;
  case MAIL:
    return mail_block;
  case MEMORY:
    return mem_block;
  case TEMP:
    return temp_block;
  case VOLUME:
    return vol_block;
  default:
    return NULL;
  }
}

const char *get_label(enum blocks id) {
  switch (id) {
  case BATTERY:
    return "BAT";
  case CLOCK:
    return NULL;
  case CPU:
    return "CPU";
  case DESKTOP:
    return NULL;
  case MAIL:
    return "MAIL";
  case MEMORY:
    return "MEM";
  case TEMP:
    return "TEMP";
  case VOLUME:
    return "VOL";
  default:
    return NULL;
  }
}

const char *get_command(enum blocks id) {
  switch (id) {
  case BATTERY:
    return NULL;
  case CLOCK:
    return NULL;
  case CPU:
    return NULL;
  case DESKTOP:
    return NULL;
  case MAIL:
    return NULL;
  case MEMORY:
    return NULL;
  case TEMP:
    return NULL;
  case VOLUME:
    return NULL;
  default:
    return NULL;
  }
}
