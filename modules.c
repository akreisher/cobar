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


void *clock_block (void *input) {
  char buf[128];
  int fifo;
  time_t t;

  block_input *in;
  clock_arg *arg;
  block_output out;

  in = (block_input *) input;
  arg = (clock_arg *) in->arg;
  out.id = in->id;
  out.data = buf;

  fifo = open(FIFO, O_WRONLY);
  
  while (1) {
    t = time(NULL);
    strftime(buf, 128, arg->time_format, localtime(&t));
    write(fifo, &out, sizeof(out));
    sleep(arg->dt);
  }
  close(fifo);
}

/* CPU Usage */
void *cpu_block (void *input) {

  char buf[128];
  FILE *cpuinfo;
  int fifo;
  unsigned long long int user, nice, sys, idle, iowait, irq, sirq, \
    steal, guest, nguest, in_use, total, old_in_use, old_total;

  block_input *in;
  cpu_arg *arg;
  block_output out;

  in = (block_input *) input;
  arg = (cpu_arg *) in->arg;
  out.id = in->id;
  out.data = buf;
  
  fifo = open(FIFO, O_WRONLY);

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
    
    sprintf(buf, "%%{F#FFFFFF} CPU %.1lf%% %%{F-}%%{B-}",
	     100.0f * (in_use - old_in_use) / (float) (total - old_total));
    write(fifo, &out, sizeof(out));

    old_in_use = in_use;
    old_total = total;
    
    sleep(arg->dt);
  }
  close(fifo);
}


struct desktop_info {
  unsigned long int id;
  char name[32];
};

void *desktop_block (void *input) {

  char buf[128], desktop[64];
  FILE *bspc_sub, *bspc_query;
  int fifo, len;
  unsigned long int monitor_id, desktop_id;
  float temp;

  block_input *in;
  block_output out;

  struct desktop_info desktops[10];

  in = (block_input *) input;
  out.id = in->id;
  out.data = buf;
  
  fifo = open(FIFO, O_WRONLY);

  // Initial desktop
  bspc_query = popen("bspc query --names -D -d", "r");
  fgets(desktop, 128, bspc_query);
  pclose(bspc_query);
  desktop[strcspn(desktop, "\r\n")] = '\0';
  sprintf(buf, "%%{F#FFFFFF} %s %%{F-}%%{B-}", desktop);
  write(fifo, &out, sizeof(out));
  
  
  bspc_sub = popen("bspc subscribe desktop_focus", "r");

  while (1) {

    fscanf(bspc_sub, "desktop_focus %lx %lx", &monitor_id, &desktop_id);
    bspc_query = popen("bspc query --names -D -d", "r");
    fgets(desktop, 64, bspc_query);
    pclose(bspc_query);
    
    desktop[strcspn(desktop, "\r\n")] = '\0';
    
    sprintf(buf, "%%{F#FFFFFF} %s %%{F-}%%{B-}", desktop);
    write(fifo, &out, sizeof(out));
  }

  pclose(bspc_sub);
  close(fifo);
}


void *mem_block (void *input) {

  char buf[128];
  FILE *meminfo;
  int fifo, mem_free, mem_tot;

  block_input *in;
  mem_arg *arg;
  block_output out;

  in = (block_input *) input;
  arg = (mem_arg *) in->arg;

  out.id = in->id;
  out.data = buf;
  
  fifo = open(FIFO, O_WRONLY);

  while (1) {

    meminfo = fopen("/proc/meminfo", "r");
    fscanf(meminfo, "MemTotal: %d kB\n", &mem_tot);
    fscanf(meminfo, "MemFree: %d kB", &mem_free);
    fclose(meminfo);
    
    sprintf(buf, "%%{F#FFFFFF} MEM %.1f G %%{F-}%%{B-}",
	    ((float) mem_free) / ((float) (1<<20)));
    write(fifo, &out, sizeof(out));
    sleep(arg->dt);
  }
  close(fifo);
}


/* Temperature */
void *temp_block (void *input) {

  char buf[128], *color;
  char command[64] = "sensors -u ";
  FILE *sensors;
  int fifo;
  float temp;

  temp_arg* arg;
  block_input* in;
  block_output out;

  in = (block_input *) input;
  arg = (struct temp_arg *) in->arg;
  out.id = in->id;
  out.data = buf;

  strncat(command, arg->chip, 48);
  
  fifo = open(FIFO, O_WRONLY);

  while (1) {

    sensors = popen(command, "r");

    while (fgets(buf, 128, sensors)) {
      if (sscanf(buf, "  temp1_input: %f", &temp))
	break;
    }
    pclose(sensors);

    if (temp > arg->T_crit)
      color = "%{F#FF0000}";
    else if (temp > arg->T_warn)
      color = "%{F#FFFC00}";
    else
      color = "%{F#FFFFFF}";

    sprintf(buf, "%s %.1f°C %%{F-}%%{B-}", color, temp);
    write(fifo, &out, sizeof(out));
    sleep(arg->dt);
  }
  close(fifo);
}

void *vol_block(void *input) {

  
  char buf[128];
  FILE *pulse;
  int fifo;
  int vol1, vol2;

  vol_arg* arg;
  block_input* in;
  block_output out;

  in = (block_input *) input;
  arg = (vol_arg *) in->arg;
  out.id = in->id;
  out.data = buf;

  fifo = open(FIFO, O_WRONLY);
  

  while (1) {
    pulse = popen("pulsemixer --get-volume", "r");
    fscanf(pulse, "%d %d\n", &vol1, &vol2);
    pclose(pulse);
    
    sprintf(buf, " VOL %d ", vol1);
    write(fifo, &out, sizeof(out));
    sleep(arg->dt);
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

