#ifndef CONFIG_H_
#define CONFIG_H_
#include <stddef.h>
#include "modules.h"

/* SETTINGS */
#define NUM_MONITORS 1
#define NUM_DESKTOPS 10

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_WARN
#endif

/* Bar Options */
#define BACKGROUND_COLOR "#000000"
#define RESOLUTION "1920x28"
#define HEIGHT 28
#define WIDTH 1920
#define SCREEN_HEIGHT 1080
#define FONT "SourceCodePro"

/* ARGS */
clock_arg clock_args = {
    .dt = 60,
    .time_format="%%{F#FFFFFF} %b %d %R %%{F-}%%{B-}"
};

cpu_arg cpu_args = {
  .dt = 2,
  .cpu_crit = 90,
  .cpu_warn = 80,
};

mem_arg mem_args = {
    .dt = 5,
};

temp_arg temp_args = {
    .dt = 2,
    .T_crit = 90,
    .T_warn = 70,
    .chip = "k10temp-pci-00c3",
};

vol_arg vol_args = {
    .dt = 5,
};

desktop_arg desktop_args = {
  .num_desktops = 10,
};


/* tray_arg tray_args = { */
/*     .dt = 0, */
/*     .x_pos = WIDTH - HEIGHT, */
/*     .y_pos = SCREEN_HEIGHT - HEIGHT, */
/*     .icon_size = HEIGHT */
/* }; */

block_def lblocks[] = {
  {desktop_block, -1},
};

block_def rblocks[] = {
  {vol_block,      2},
  {temp_block,    -1},
  {cpu_block,     -1},
  {clock_block,   -1},
  // { mem_block,    (void *) &mem_args,    NULL },
  // { tray_block,   (void *) &tray_args,   NULL },
};

#endif
