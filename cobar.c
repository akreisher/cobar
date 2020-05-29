#include <fcntl.h>
#include <errno.h>
#include <libgen.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log/log.h"
#include "modules.h"
#include "config.h"

char *bar_command[] = {
  "lemonbar",
  "-B", BACKGROUND_COLOR,
  "-g", RESOLUTION,
  "-f", FONT,
  NULL};

// Epoll Events
#define MAX_EVENTS 10

// Pipes for signal handling.
#define N_SIGRT 10
static int sig_pipes[N_SIGRT];
static sigset_t sig_mask;

pthread_mutex_t log_lock;


// A module object
typedef struct block_module {
  const block_def *def;
  block_input input;
  pthread_t thread;
  int pipes[4];
  char data[512];
} block_module;

enum block_pipe {
  BLOCK_READ,
  BAR_WRITE,
  BAR_READ,
  BLOCK_WRITE
};


void process_command(const char *buf) {
  FILE *bspc_fd;
  char cmd[64];
  unsigned long int desktop_id;

  if ((sscanf(buf, "desktop %lX\n", &desktop_id) == 1)) {
    snprintf(cmd, 64, "bspc desktop --focus 0x%lX", desktop_id);
    log_info("%s", cmd);
    bspc_fd = popen(cmd, "r");
    pclose(bspc_fd);
  }
}

void sigrt_handler (int signum) {
  log_info("Received SIGRTMIN+%d", signum-SIGRTMIN);
  write(sig_pipes[signum-SIGRTMIN], "", 1);
}


int setup_signal(int num, int pipe) {
  // Setup RT signals
  struct sigaction act = {
    .sa_handler = sigrt_handler,
    .sa_flags = SA_RESTART
  };

  if (sigaction(SIGRTMIN+num, &act, NULL) < 0) {
    perror("Setting rt signal");
    exit(EXIT_FAILURE);
  }
  if (sigaddset(&sig_mask, SIGRTMIN + num) == -1) {
    perror("sigaddset");
    exit(EXIT_FAILURE);
  }
  sig_pipes[num] = pipe;
  return 0;
}

void init_block(const block_def *def, block_module *block, int id, int epfd) {
  struct epoll_event ev;
  ev.events = EPOLLIN;

  block->def = def;
  block->input.id = id;

  // Set up pipes
  pipe(block->pipes);
  pipe(block->pipes + 2);
  block->input.infd = block->pipes[BLOCK_READ];
  block->input.outfd = block->pipes[BLOCK_WRITE];

  if (def->sigrt_num > 0 && def->sigrt_num < N_SIGRT) {
    setup_signal(def->sigrt_num, block->pipes[BAR_WRITE]);
  }

  
  // Add out pipe read fd to epoll
  ev.data.fd = block->pipes[BAR_READ];

  // Don't block write pipes.
  if (fcntl(block->pipes[BAR_WRITE], F_SETFL, O_NONBLOCK) == -1) {
    perror("Set bar_write non blocking.");
    exit(EXIT_FAILURE);
  }
  if (fcntl(block->pipes[BLOCK_WRITE], F_SETFL, O_NONBLOCK) == -1) {
    perror("Set block_write non blocking.");
    exit(EXIT_FAILURE);
  }

  if (epoll_ctl(epfd, EPOLL_CTL_ADD, block->pipes[BAR_READ], &ev) == -1) {
    perror("epoll_ctl: add block out pipe");
    exit(EXIT_FAILURE);
  }

  // Start threads
  log_info("Starting thread %d", id);
  pthread_create(&block->thread, NULL, def->func, (void *) &block->input);
}


int main () {
  int i, nfds, epoll_fd, bar_pid, bar_pipes[4];
  int num_lblocks = sizeof(lblocks)/sizeof(block_def);
  int num_rblocks = sizeof(rblocks)/sizeof(block_def);
  FILE *bar_fd;
  ssize_t nb;
  char BUF[512];
  struct epoll_event ev, events[MAX_EVENTS];

  block_module lblock_mods[num_lblocks];
  block_module rblock_mods[num_rblocks];
  block_output output;


  // Logging setup
  log_init();
  log_set_level(LOG_LEVEL);

  // Start lemonbar
  log_info("Starting lemonbar");
  pipe(bar_pipes);
  pipe(bar_pipes+2);
  bar_pid = fork();

  if (bar_pid == 0) {
    // Bar process
    close(bar_pipes[0]); // read end of read-pipe
    close(bar_pipes[3]); // write end of write-pipe

    dup2(bar_pipes[1], STDOUT_FILENO);
    dup2(bar_pipes[1], STDERR_FILENO);
    dup2(bar_pipes[2], STDIN_FILENO);

    close(bar_pipes[1]);
    close(bar_pipes[2]);

    // ask kernel to deliver SIGTERM in case the parent dies
    prctl(PR_SET_PDEATHSIG, SIGTERM);
    
    execvp("lemonbar", bar_command);
    perror("Exec lemonbar");
    exit(EXIT_FAILURE);
  }
  
  // Setup epoll
  if ((epoll_fd = epoll_create1(0)) == -1) {
    perror("epoll_create1");
    exit(EXIT_FAILURE);
  }

  // Bar stdout
  ev.events = EPOLLIN;
  ev.data.fd = bar_pipes[0];
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, bar_pipes[0], &ev) == -1) {
    perror("epoll_ctl: add block out pipe");
    exit(EXIT_FAILURE);
  }

  // Block inits
  for (i = 0; i < num_lblocks; i++){
    init_block(&lblocks[i], &lblock_mods[i], i, epoll_fd);
  }
  for (i = 0; i < num_rblocks; i++){
    init_block(&rblocks[i], &rblock_mods[i], i+num_lblocks, epoll_fd);
  }

  while (1) {
    /* Wait for update */
    nfds = epoll_pwait(epoll_fd, events, MAX_EVENTS, -1, &sig_mask);
    if (nfds == -1 && errno != EINTR) {
      perror("epoll_wait");
      exit(EXIT_FAILURE);
    }

    for (i = 0; i < nfds; i++) {
      if (events[i].data.fd == bar_pipes[0]) {
	/* Mouse command from bar */
	nb = read(events[i].data.fd, BUF, 512);
	if (nb < 0) {
	  perror("read bytes");
	  exit(EXIT_FAILURE);
	}
	BUF[nb] = '\0';
	process_command(BUF);
	continue;
      }

      /* Block input */
      if (read(events[i].data.fd, &output, sizeof(block_output)) < 0) {
	perror("read bytes");
	exit(EXIT_FAILURE);
      }
      log_debug("Got %s from %d", output.data, output.id);
      strncpy(output.id < num_lblocks ? lblock_mods[output.id].data
	      : rblock_mods[output.id-num_lblocks].data,
	      output.data, 512);
    }
    /* Left Blocks */
    write(bar_pipes[3], "%{l}", 4);
    for (int i = 0; i < num_lblocks; i++) {
      if (i) write(bar_pipes[3], "|", 1);
      write(bar_pipes[3], lblock_mods[i].data, strlen(lblock_mods[i].data));
    }

    /* Right Blocks */
    write(bar_pipes[3], "%{r}", 4);
    for (int i = 0; i < num_rblocks; i++) {
      if (i) write(bar_pipes[3], "|", 1);
      write(bar_pipes[3], rblock_mods[i].data, strlen(rblock_mods[i].data));
    }

    /* Write to bar_pipes[3] */
    write(bar_pipes[3], "\n", 1);
  }
  
  pclose(bar_fd);
  pthread_mutex_destroy(&log_lock);
  return 0;
}
