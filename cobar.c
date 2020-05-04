#include <fcntl.h> 
#include <libgen.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <string.h> 
#include <sys/stat.h> 
#include <sys/types.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "modules.h"
#include "config.h"

#define BAR								\
  "lemonbar"								\
  " -B " BACKGROUND_COLOR " -g " RESOLUTION " -f " FONT

// Number of signals.
#define N_SIGRT 10

// Pipes for signal handling.
static int sig_pipes[N_SIGRT];

void sig_handler (int signum) {
#ifdef DEBUG
  printf("Received signal: %d\n", signum);
#endif
  write(sig_pipes[(signum - SIGRTMIN)*2 + 1], "", 1);
}


int setup_signals() {
  // Setup RT signals
  int i;
  struct sigaction act = {
    .sa_handler = sig_handler,
    .sa_flags = SA_RESTART
  };

  for (i = 0; i < N_SIGRT; i++) {
    pipe(sig_pipes + 2*i);  // read pipes
    // Don't block write pipes.
    fcntl(sig_pipes[2*i + 1], F_SETFL, O_NONBLOCK);
    if (sigaction(SIGRTMIN+i, &act, NULL) < 0) {
      perror("sigaction");
      return 1;
    }
  }
  return 0;
}


int main () {
  int i, fifo;
  int num_lblocks = sizeof(lblocks)/sizeof(bar_module);
  int num_rblocks = sizeof(rblocks)/sizeof(bar_module);
  FILE *bar;
  ssize_t nb;
  char BUF[1024], *BIN_DIR;

  pthread_t lthreads[num_lblocks];
  block_input linputs[num_lblocks];
  pthread_t rthreads[num_rblocks];
  block_input rinputs[num_rblocks];

  block_output block_buf;

  setup_signals();
  if (mkfifo(FIFO, 0666) < 0) perror("fifo");
  
  bar = popen(BAR, "w");

  for (i = 0; i < num_lblocks; i++){
    linputs[i].id = i;
    if (lblocks[i].sig_num > 0) {
      linputs[i].sig_pipe = sig_pipes[2 * lblocks[i].sig_num];
    }
    linputs[i].arg = lblocks[i].arg;
    pthread_create(&lthreads[i], NULL, lblocks[i].func,
		   (void *) &linputs[i]);
  }

  for (i = 0; i < num_rblocks; i++){
    if (rblocks[i].sig_num > 0) {
      rinputs[i].sig_pipe = sig_pipes[2 * rblocks[i].sig_num];
    }
    rinputs[i].id = num_lblocks + i;
    rinputs[i].arg = rblocks[i].arg;
    pthread_create(&rthreads[i], NULL, rblocks[i].func,
		   (void *) &rinputs[i]);
  }
  
  fifo = open(FIFO, O_RDONLY);
  
  while (1) {
    /* Wait for update */

    nb = read(fifo, &block_buf, sizeof(block_buf));
    if (nb < 0) perror(NULL);

#ifdef DEBUG
    printf("Got %s from %d\n", block_buf.data, block_buf.id);
#endif
    
    if (block_buf.id < num_lblocks)
      lblocks[block_buf.id].data = block_buf.data;
    else
      rblocks[block_buf.id - num_lblocks].data = block_buf.data;

    
    /* Left Blocks */
    fputs("%{l}", bar);
    for (int i = 0; i < num_lblocks; i++) {
      if (lblocks[i].data != NULL) fputs(lblocks[i].data, bar);
    } 
    
    /* Right Blocks */
    fputs("%{r}", bar);
    for (int i = 0; i < num_rblocks; i++) {
      if (i) fputs("|", bar);
      if (rblocks[i].data != NULL) fputs(rblocks[i].data, bar);
    }

    /* Write to bar */
    fputs("\n", bar);
    fflush(bar);
  }
  
  close(fifo);
  pclose(bar);
  return 0;
}
