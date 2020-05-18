#ifndef CONFIG_H_
#define CONFIG_H_
#include <stddef.h>
#include "modules.h"

#define DEBUG

/* SETTINGS */

/* Bar Options */
#define BACKGROUND_COLOR "#000000"
#define RESOLUTION "3840x28"
#define HEIGHT 28
#define WIDTH 1920
#define SCREEN_HEIGHT 1080
#define FONT "SourceCodePro:size=8"

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

mail_arg mail_args = {
  .command = "mu find date:1w..now maildir:/INBOX flag:unread 2>/dev/null | wc -l"
};

mem_arg mem_args = {
  .dt = 5,
};

temp_arg temp_args = {
    .dt = 2,
    .T_crit = 90,
    .T_warn = 70,
    .chip = "thinkpad-isa-0000",
};

temp_arg vol_args = {
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
  {desktop_block, (void *)&desktop_args, -1},
};

block_def rblocks[] = {
  {mail_block,   (void *) &mail_args,     9},
  {vol_block,    (void *) &vol_args,      2},
  {temp_block,   (void *) &temp_args,    -1},
  {cpu_block,    (void *) &cpu_args,     -1},
  {clock_block,  (void *) &clock_args,   -1},
  // { mem_block,    (void *) &mem_args,    NULL },
  // { tray_block,   (void *) &tray_args,   NULL },
};

#endif
