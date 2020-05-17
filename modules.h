#ifndef MODULE_H_
#define MODULE_H_
#include <pthread.h>

// Config definition of a module
typedef struct block_def {
  void *(*func) (void *);  // Function
  void *arg;
  int sigrt_num;  // Signal to handle
} block_def;

// Input to a block module
typedef struct block_input {
  int id;
  int *in_pipe;
  int *out_pipe;
  int sig_pipe;
  void *arg;  // Block-specific
} block_input;

// A module object
typedef struct block_module {
  const block_def *def;
  block_input input;
  pthread_t thread;
  int in_pipe[2];  // 0: read; 1: write
  int out_pipe[2];  // 0: read; 1: write
  char data[512];
} block_module;

// Output of a block module
typedef struct block_output {
  int id;
  int fd;
  char data[512];
} block_output;



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
