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

void init_output(const block_input *input, block_output *output) {
  output->id = input->id;
  output->fd = input->outfd;
}

void write_data(const block_output *output) {
  write(output->fd, output, sizeof(block_output));
}

void *battery_block(void *input) {
  int cap;
  char *color;
  FILE *f;

  block_output out;
  block_input *in = (block_input *) input;
  init_output(in, &out);
  while (1) {
    f = fopen("/sys/class/power_supply/BAT0/capacity" , "r");
    fscanf(f, "%d", &cap);
    fclose(f);

    if      (cap <= battery_args.bat_crit)  color = "%{F#FF0000}";
    else if (cap <= battery_args.bat_warn)  color = "%{F#FFFC00}";
    else                                    color = "%{F#FFFFFF}";

    snprintf(out.data, 64, " BAT %s%d%% %%{F-}%%{B-}", color, cap);
    write_data(&out);

    sleep(battery_args.dt);
  }
}

void *clock_block (void *input) {
  time_t t;

  block_output out;
  block_input *in = (block_input *) input;
  init_output(in, &out);

  while (1) {
    t = time(NULL);
    strftime(out.data, 64, clock_args.time_format, localtime(&t));
    write_data(&out);
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

  block_input *in;
  block_output out;

  in = (block_input *) input;
  init_output(in, &out);

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
    if      (percent > cpu_args.cpu_crit)  color = "%{F#FF0000}";
    else if (percent > cpu_args.cpu_warn)  color = "%{F#FFFC00}";
    else                                   color = "%{F#FFFFFF}";

    snprintf(out.data, 64, " CPU %s%.1lf%% %%{F-}%%{B-}", color, percent);
    write_data(&out);

    old_in_use = in_use;
    old_total = total;

    sleep(cpu_args.dt);
  }
}


void *mail_block (void *input) {

  FILE *mail_fd;
  int unread;

  block_input *in;
  block_output out;

  in = (block_input *) input;
  init_output(in, &out);

  while (1) {
    mail_fd = popen(mail_args.command, "r");
    fscanf(mail_fd, "%d\n", &unread);
    pclose(mail_fd);

    sprintf(out.data, "%%{A:mail:} MAIL %d %%{A}", unread);
    write_data(&out);
    usleep(2000);
    read(in->infd, out.data, 1);
  }

  
}

void *mem_block (void *input) {

  FILE *meminfo_fd;
  int mem_free, mem_tot;

  block_input *in;
  block_output out;

  in = (block_input *) input;
  init_output(in, &out);

  while (1) {

    meminfo_fd = fopen("/proc/meminfo", "r");
    fscanf(meminfo_fd, "MemTotal: %d kB\nMemFree: %d kB", &mem_tot, &mem_free);
    fclose(meminfo_fd);

    snprintf(out.data, 64, "%%{F#FFFFFF} MEM %.1f G %%{F-}%%{B-}",
	    ((float) mem_free) / ((float) (1<<20)));
    write_data(&out);
    sleep(mem_args.dt);
  }
}


/* Temperature */
void *temp_block (void *input) {

  char *color;
  char command[64] = "sensors -u ";
  FILE *sensors_fd;
  float temp;

  block_input* in;
  block_output out;

  in = (block_input *) input;
  init_output(in, &out);

  strncat(command, temp_args.chip, 48);

  while (1) {

    sensors_fd = popen(command, "r");

    while (fgets(out.data, 64, sensors_fd)) {
      if (sscanf(out.data, "  temp1_input: %f", &temp))
	break;
    }
    pclose(sensors_fd);

    if (temp > temp_args.T_crit)
      color = "%{F#FF0000}";
    else if (temp > temp_args.T_warn)
      color = "%{F#FFFC00}";
    else
      color = "%{F#FFFFFF}";

    snprintf(out.data, 64, "%s %.1f°C %%{F-}%%{B-}", color, temp);
    write_data(&out);
    sleep(temp_args.dt);
  }
}

void *vol_block(void *input) {

  FILE *pulse_fd;
  int vol[2];

  block_input* in;
  block_output out;

  in = (block_input *) input;
  init_output(in, &out);

  while (1) {
    pulse_fd = popen("pulsemixer --get-volume", "r");
    fscanf(pulse_fd, "%d %d\n", vol, vol+1);
    pclose(pulse_fd);

    sprintf(out.data, " VOL %d ", vol[0]);
    write_data(&out);
    usleep(2000);
    read(in->infd, out.data, 1);
  }
}


/* void *systray(void *input) { */
/*   char buf[256], desktop[64]; */
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
