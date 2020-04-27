#ifndef CONFIG_H_
#define CONFIG_H_
#include <stddef.h>
#include "modules.h"

/* SETTINGS */


/* Bar Options */
#define BACKGROUND_COLOR "'#000000'"
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
  .dt = 1,
};

mem_arg mem_args = {
    .dt = 5,
};

temp_arg temp_args = {
    .dt = 1,
    .T_crit = 90,
    .T_warn = 70,
    .chip = "k10temp-pci-00c3",
};

temp_arg vol_args = {
    .dt = 5,
};


/* tray_arg tray_args = { */
/*     .dt = 0, */
/*     .x_pos = WIDTH - HEIGHT, */
/*     .y_pos = SCREEN_HEIGHT - HEIGHT, */
/*     .icon_size = HEIGHT */
/* }; */



bar_module lblocks[] = {
        {desktop_block, NULL, NULL},
};

bar_module rblocks[] = {
  {vol_block, (void *)&vol_args, NULL},
  {temp_block, (void *)&temp_args, NULL},
  {cpu_block, (void *)&cpu_args, NULL},
  { clock_block,  (void *) &clock_args,  NULL },
  // { mem_block,    (void *) &mem_args,    NULL },
  // { tray_block,   (void *) &tray_args,   NULL },
};

#endif
