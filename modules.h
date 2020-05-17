#ifndef MODULE_H_
#define MODULE_H_

// Config definition of a module
typedef struct block_def {
  void *(*func) (void *);  // Function
  int sigrt_num;  // Signal to handle
} block_def;

// Input to a block module
typedef struct block_input {
  int id;
  int infd;
  int outfd;
} block_input;

// Output of a block module
typedef struct block_output {
  int id;
  int fd;
  char data[512];
} block_output;

void init_output(const block_input *input, block_output *output);
void write_data(const block_output *output) ;

/***********BLOCKS************/

/*           CLOCK           */
void *clock_block(void *input);
typedef struct clock_arg {
  int dt;
  const char *time_format;
} clock_arg;
extern clock_arg clock_args;

/*             CPU           */
void *cpu_block(void *input);
typedef struct cpu_arg {
  int dt;
  float cpu_crit, cpu_warn;
} cpu_arg;
extern cpu_arg cpu_args;


/*           DESKTOP         */
void *desktop_block(void *input);
typedef struct desktop_arg {
  int num_desktops;
} desktop_arg;
extern desktop_arg desktop_args;


/*           MEMORY          */
void *mem_block(void *input);
typedef struct mem_arg {
  int dt;
} mem_arg;
extern mem_arg mem_args;


/*        TEMPERATURE        */
void *temp_block(void *input);
typedef struct temp_arg {
  int dt;
  float T_crit, T_warn;
  const char *chip;
} temp_arg;
extern temp_arg temp_args;


/*          VOLUME          */
void *vol_block(void *input);
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
