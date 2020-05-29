#include <fcntl.h>
#include <errno.h>
#include <getopt.h>
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
  "-a", "20",
  NULL};

// Number of signals.
#define N_SIGRT 10

#define MAX_EVENTS 10

// Pipes
static int bar_pipes[4];
static int sig_pipes[N_SIGRT];
static sigset_t sig_mask;

pthread_mutex_t log_lock;
static FILE *log_file;


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


void
log_lock_fn(void *udata, int lock) {
  if (lock)
    pthread_mutex_lock((pthread_mutex_t *) udata);
  else
    pthread_mutex_unlock((pthread_mutex_t *) udata);
}


static inline void
init_log(const char *log_level, const char *log_filename) {
  int level = LOG_WARN;

  if (log_level) {
    if (!strcmp(log_level, "trace") || !strcmp(log_level, "TRACE"))
      level = LOG_TRACE;
    else if (!strcmp(log_level, "debug") || !strcmp(log_level, "DEBUG"))
      level = LOG_DEBUG;
    else if (!strcmp(log_level, "info") || !strcmp(log_level, "INFO"))
      level = LOG_INFO;
    else if (!strcmp(log_level, "warn") || !strcmp(log_level, "WARN"))
      level = LOG_WARN;
    else if (!strcmp(log_level, "error") || !strcmp(log_level, "ERROR"))
      level = LOG_ERROR;
    else if (!strcmp(log_level, "fatal") || !strcmp(log_level, "FATAL"))
      level = LOG_FATAL;
    else if (!strcmp(log_level, "error") || !strcmp(log_level, "ERROR"))
      level = LOG_ERROR;
  }
  
  pthread_mutex_init(&log_lock, NULL);
  log_set_lock(log_lock_fn);
  log_set_udata(&log_lock);
  log_set_level(level);

  if (log_filename) {
    log_file = fopen(log_filename, "w");
    if (!log_file)
      log_error("Unable to open log file %s: %s", log_filename,
                strerror(errno));
    else
      log_set_fp(log_file);
  }
}


static inline void
process_command(const char *buf) {
  FILE *bspc_fd;
  char cmd[64];
  unsigned long int desktop_id;
  log_debug("Got command: %s", buf);

  if ((sscanf(buf, "desktop %lX", &desktop_id) == 1)) {
    snprintf(cmd, 64, "bspc desktop --focus 0x%lX", desktop_id);
    log_info("Running command: \"%s\"", cmd);
    bspc_fd = popen(cmd, "r");
    pclose(bspc_fd);
  } else if (strcmp(buf, "mail") == 0) {
    bspc_fd = popen("bspc desktop --focus mail", "r");
    pclose(bspc_fd);
  }
}

void
sigrt_handler (int signum) {
  log_info("Received SIGRTMIN+%d", signum-SIGRTMIN);
  write(sig_pipes[signum-SIGRTMIN], "", 1);
}


static inline int
setup_signal(int num, int pipe) {
  // Setup RT signals
  struct sigaction act = {
    .sa_handler = sigrt_handler,
    .sa_flags = SA_RESTART
  };

  if (sigaction(SIGRTMIN+num, &act, NULL) < 0) {
    perror("Setting rt signal");
    exit(EXIT_FAILURE);
  }
  if (sigaddset(&sig_mask, SIGRTMIN+num) == -1) {
    perror("sigaddset");
    exit(EXIT_FAILURE);
  }
  sig_pipes[num] = pipe;
  return 0;
}

static inline void
init_block(const block_def *def, block_module *block, int id, int epfd) {
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

static inline void
init_bar() {
  log_info("Starting lemonbar");

  // Set up pipes.
  pipe(bar_pipes);
  pipe(bar_pipes+2);
  
  if (fork() == 0) {
    // Bar process

    dup2(bar_pipes[1], STDOUT_FILENO);
    dup2(bar_pipes[1], STDERR_FILENO);
    dup2(bar_pipes[2], STDIN_FILENO);

    // Close irrelevant fd's.
    close(bar_pipes[0]);
    close(bar_pipes[1]);
    close(bar_pipes[2]);
    close(bar_pipes[3]);
    
    // ask kernel to deliver SIGTERM in case the parent dies
    prctl(PR_SET_PDEATHSIG, SIGTERM);
    
    execvp("lemonbar", bar_command);

    // If we get here, it's a failure.
    log_fatal("Exec lemonbar: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }
}


int main (int argc, char *argv[]) {
  int i, nfds, epoll_fd;
  int num_lblocks = sizeof(lblocks)/sizeof(block_def);
  int num_rblocks = sizeof(rblocks)/sizeof(block_def);
  FILE *bar_fd;
  ssize_t nb;
  char BUF[512], opt, *log_level=NULL, *log_filename=NULL;
  struct epoll_event ev, events[MAX_EVENTS];

  block_module lblock_mods[num_lblocks];
  block_module rblock_mods[num_rblocks];
  block_output output;

  static struct option long_opts[] = {
    {"log-level", required_argument, 0, 'g'},
    {"log-file", required_argument, 0, 'f'},
  };

  while (1) {

    opt = getopt_long(argc, argv, "g:f:", long_opts, NULL);

    if (opt == -1) break;

    switch (opt) {
    case 'g':
      log_level = optarg;
      break;
    case 'f':
      log_filename = optarg;
      break;
    case '?':
      break;
    }
  }

  // Logging setup
  init_log(log_level, log_filename);

  
  while (optind < argc)
    log_warn("Ignoring unrecognized argument: %s", argv[optind++]);

  // Get options



  // Start lemonbar
  init_bar();
  
  // Setup epoll
  epoll_fd = epoll_create1(0);

  if (epoll_fd == -1) {
    perror("epoll_create1");
    exit(EXIT_FAILURE);
  }

  for (i = 0; i < num_lblocks; i++){
    init_block(&lblocks[i], &lblock_mods[i], i, epoll_fd);
  }

  for (i = 0; i < num_rblocks; i++){
    init_block(&rblocks[i], &rblock_mods[i], i+num_lblocks, epoll_fd);
  }

  ev.events = EPOLLIN;
  ev.data.fd = bar_pipes[0];

  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, bar_pipes[0], &ev) == -1) {
    perror("epoll_ctl: add block out pipe");
    exit(EXIT_FAILURE);
  }

  int pid;


  while (1) {
    /* Wait for update */
    nfds = epoll_pwait(epoll_fd, events, MAX_EVENTS, -1, &sig_mask);
    if (nfds == -1 && errno != EINTR) {
      perror("epoll_wait");
      exit(EXIT_FAILURE);
    }

    for (i = 0; i < nfds; i++) {
      if (events[i].data.fd == bar_pipes[0]) {
	nb = read(events[i].data.fd, BUF, 512);
	if (nb < 0) {
	  perror("read bytes");
	  exit(EXIT_FAILURE);
	}
	BUF[strcspn(BUF, "\r\n")] = '\0';
	process_command(BUF);
	continue;
      }

      nb = read(events[i].data.fd, &output, sizeof(block_output));
      if (nb < 0) {
	perror("read bytes");
	exit(EXIT_FAILURE);
      }

      log_info("%d-Received: \"%s\"", output.id, output.data);

      strncpy(output.id < num_lblocks ? lblock_mods[output.id].data
	      : rblock_mods[output.id-num_lblocks].data,
	      output.data, 512);
    }

    // HACK: Should modules handle monitors individually?
    write(bar_pipes[3], "%{l}", 4);
    for (int i = 0; i < num_lblocks; i++) {
      write(bar_pipes[3], lblock_mods[i].data,
	    strlen(lblock_mods[i].data));
    }

    write(bar_pipes[3], "%{S0}", 5);
    for (int m = 0; m < NUM_MONITORS; m++) {
      /* Left Blocks */

      write(bar_pipes[3], "%{r}", 4);
      for (int i = 0; i < num_rblocks; i++) {
	if (i) write(bar_pipes[3], "|", 1);
	write(bar_pipes[3], rblock_mods[i].data,
	      strlen(rblock_mods[i].data));
      }
      write(bar_pipes[3], "%{S+}", 5);
    }
    
    /* Write to bar_pipes[3] */
    write(bar_pipes[3], "\n", 1);
  }
  
  pclose(bar_fd);
  pthread_mutex_destroy(&log_lock);
  return 0;
}
