#ifndef MODULE_H_
#define MODULE_H_

#define MAX_MONITORS 1
#define MAX_DESKTOPS 10  /* Per monitor */

#define TEXT_LEN 32

enum blocks {
  BATTERY = 0,
  CLOCK,
  CPU,
  DESKTOP,
  MAIL,
  MEMORY,
  TEMP,
  VOLUME,
  BLOCK_COUNT
};


// External Config definition of a module
typedef struct block_def {
  enum blocks id;
  int signum;
  } block_def;

// Text block
typedef struct block_text {
  char label[TEXT_LEN];
  char text[TEXT_LEN];
  char command[TEXT_LEN];
  int color;
  int monitor;
} block_text;

// Input to a block module
typedef struct block_input {
  enum blocks id;
  int nt;
  int pipes[2];
} block_input;

// Internal block state
typedef struct block_internal {
  enum blocks id;
  int pipes[2];
  int nt;
  block_text *text;
} block_internal;


/* Initialize block data */
void init_internal(const void *input, block_internal *internal);
void init_text(block_text *text);
/* Write blcok data to main thread */
void write_data(const block_internal *output);

void *(*get_block_func (enum blocks id))(void *);


#define SET_LABEL(text, l) strncpy(text.label, l ? l : "", TEXT_LEN)
#define SET_COMMAND(text, c) strncpy(text.command, c ? c : "", TEXT_LEN)
#define SET_COLOR(text, c) text.color = c


/* BLOCKS */

/* BATTERY */
typedef struct battery_arg {
  int dt;
  int bat_crit, bat_warn;
} battery_arg;
extern battery_arg battery_args;

/* CLOCK */
typedef struct clock_arg {
  int dt;
  const char *time_format;
} clock_arg;
extern clock_arg clock_args;

/* CPU */
typedef struct cpu_arg {
  int dt;
  float cpu_crit, cpu_warn;
} cpu_arg;
extern cpu_arg cpu_args;


/* DESKTOP */
extern void *desktop_block(void *input);
typedef struct desktop_arg {
  int nd;
  
} desktop_arg;
extern desktop_arg desktop_args;

/* MAIL */
typedef struct mail_arg {
  const char *command;
} mail_arg;
extern mail_arg mail_args;


/* MEMORY */
typedef struct mem_arg {
  int dt;
} mem_arg;
extern mem_arg mem_args;


/* TEMPERATURE */
typedef struct temp_arg {
  int dt;
  float T_crit, T_warn;
  const char *chip;
} temp_arg;
extern temp_arg temp_args;


/* VOLUME */
typedef struct vol_arg {
  int dt;
} vol_arg;
extern vol_arg vol_args;

/*
typedef struct tray_arg {
  int dt;
  int x_pos;
  int y_pos;
  int icon_size;
} tray_arg;
void *tray_block(void *input);
*/
#endif
