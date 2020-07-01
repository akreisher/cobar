#ifndef CONFIG_H_
#define CONFIG_H_
#include <stddef.h>
#include "modules.h"

/* SETTINGS */

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_WARN
#endif

/* Bar Options */
#define BACKGROUND_COLOR "#00000000"
#define RESOLUTION "1890x28+15+3"
#define HEIGHT 28
#define WIDTH 1920
#define SCREEN_HEIGHT 1080
#define FONT "SourceCodePro"

/* ARGS */
battery_arg battery_args = {
  .dt = 5,
  .bat_crit = 10,
  .bat_warn = 30,
};

clock_arg clock_args = {
  .dt = 1,
  .time_format = "%b %d %T"
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
    .chip = "k10temp-pci-00c3",
};

vol_arg vol_args = {
    .dt = 5,
};

desktop_arg desktop_args = {
  .nd = 10,
};


block_def left_blocks[] = {
  {DESKTOP, -1}
};


/* tray_arg tray_args = { */
/*     .dt = 0, */
/*     .x_pos = WIDTH - HEIGHT, */
/*     .y_pos = SCREEN_HEIGHT - HEIGHT, */
/*     .icon_size = HEIGHT */
/* }; */


block_def right_blocks[] = {
  {MAIL,     9},
  {VOLUME,   2},
  {TEMP,    -1},
  {CPU,     -1},
  {CLOCK,   -1},
};

#endif
