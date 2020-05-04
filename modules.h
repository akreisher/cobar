#ifndef MODULE_H_
#define MODULE_H_

/* Name of pipe to bar */
#define FIFO "/tmp/bar-fifo"

/* Output Flags */
#define BAR_RESIZE 1


typedef struct bar_module {
  void *(*func) (void *);
  void *arg;
  int sig_num;
  char *data;
} bar_module;

typedef struct block_input {
  int id;
  int sig_pipe;
  void *arg;
} block_input;

typedef struct block_output {
  int id;
  int flags;
  char *data;
} block_output;


/***********BLOCKS************/


/*           CLOCK           */
typedef struct clock_arg {
  int dt;
  const char *time_format;
} clock_arg;
void *clock_block(void *input);



/*             CPU           */
typedef struct cpu_arg {
  int dt;
} cpu_arg;
void *cpu_block(void *input);


/*           DESKTOP         */
typedef struct desktop_arg {
  int num_desktops;
} desktop_arg;
void *desktop_block(void *input);



/*           MEMORY          */
typedef struct mem_arg {
  int dt;
} mem_arg;
void *mem_block(void *input);



/*        TEMPERATURE        */
typedef struct temp_arg {
  int dt;
  float T_crit, T_warn;
  const char *chip;
} temp_arg;
void *temp_block(void *input);



/*          VOLUME          */
typedef struct vol_arg {
  int dt;
} vol_arg;
void *vol_block(void *input);


typedef struct tray_arg {
  int dt;
  int x_pos;
  int y_pos;
  int icon_size;
} tray_arg;
void *tray_block(void *input);

#endif
