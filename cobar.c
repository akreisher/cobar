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

// Epoll Events
#define MAX_EVENTS 10


/* Temporary buffer */
#define BUF_SIZE 512

// Pipes
static int bar_pipes[4];

#define N_SIGRT 10
static int sig_pipes[N_SIGRT];
static sigset_t sig_mask;

// A module object
typedef struct block_module {
  block_input input;
  pthread_t thread;
  int pipes[4];
  int nt;
  block_text *text;
} block_module;

/* Block <-> Bar */
enum  {
  BLOCK_READ    = 0,
  BAR_WRITE     = 1,
  BAR_READ      = 2,
  BLOCK_WRITE   = 3
};


static inline void
process_command(const char *command) {
  FILE *f;
  char cmd[64];
  char buf[TEXT_LEN];
  log_debug("Got command: %s", command);

  if ((sscanf(command, "desktop %s", buf) == 1)) {
    snprintf(cmd, 64, "bspc desktop --focus %s", buf);
  }

  log_info("Running command: \"%s\"", cmd);
  f = popen(cmd, "r");
  pclose(f);
}

static void
sigrt_handler (int signum) {
  log_info("Received SIGRTMIN+%d", signum-SIGRTMIN);
  write(sig_pipes[signum-SIGRTMIN], "", 1);
}


static inline int
setup_signal(int num, int pipe) {
  // Setup RT signals
  log_info("Setting up signal SIGRTMIN+%d", num);
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

static void
init_block(const block_def *def, block_module *block, int epfd) {
  struct epoll_event ev;

  block->input.id = def->id;

  if (def->id == DESKTOP)
    block->nt = MAX_DESKTOPS * MAX_MONITORS;
  else
    block->nt = 1;

  block->input.nt = block->nt;
  block->text = malloc(block->nt * sizeof(block_text));

  for (int i = 0; i < block->nt; i++) {
	init_text(&block->text[i]);
  }

  /* Pipe setup */
  pipe(block->pipes);
  pipe(block->pipes + 2);
  block->input.pipes[0] = block->pipes[BLOCK_READ];
  block->input.pipes[1] = block->pipes[BLOCK_WRITE];

  /* Don't block write pipes */
  if (fcntl(block->pipes[BAR_WRITE], F_SETFL, O_NONBLOCK) == -1) {
    perror("Set bar_write non blocking.");
    exit(EXIT_FAILURE);
  }
  if (fcntl(block->pipes[BLOCK_WRITE], F_SETFL, O_NONBLOCK) == -1) {
    perror("Set block_write non blocking.");
    exit(EXIT_FAILURE);
  }

  
  // Add out pipe read fd to epoll
  ev.events = EPOLLIN;
  ev.data.fd = block->pipes[BAR_READ];
  
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, block->pipes[BAR_READ], &ev) == -1) {
    perror("epoll_ctl: add block out pipe");
    exit(EXIT_FAILURE);
  }
  
  /* Signal Setup */
  if (def->signum > 0 && def->signum < N_SIGRT) {
    setup_signal(def->signum, block->pipes[BAR_WRITE]);
  }

  // Start threads
  log_info("Starting thread %d", def->id);
  pthread_create(&block->thread, NULL, get_block_func(def->id), (void *) &block->input);
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

 

static inline void
write_text(const block_text *text, int barfd) {
  int i, max;
  if (text->monitor == -1) {
    i = 0;
    max = MAX_MONITORS;
  } else {
    i = text->monitor;
    max = text->monitor + 1;
  }

  for (; i < max; i++) {

	log_info("Writing str %s, command %s", text->text, text->command);

    if (*text->command) dprintf(barfd, "%%{A:%s:}", text->command);
    if (*text->label) dprintf(barfd, "%s ", text->label);
    /* dprintf(barfd, "%%{S%d}%%{F#%x}%s%%{F-}", i, text->color, text->text); */
	dprintf(barfd, "%%{F#%x}%s%%{F-}", text->color, text->text);
	if (*text->command) dprintf(barfd, "%%{A}");
  }
}

static inline void
write_module(const block_module *mod, int barfd) {
  for (int i = 0; i < mod->nt; i++) {
    write_text(&mod->text[i], barfd);
  }
}

int main (int argc, char *argv[]) {
  int i, j, nfds, epoll_fd, nr, nl;
  enum blocks id;
  block_text *text;
  FILE *bar_fd;
  ssize_t nb;
  char buf[BUF_SIZE], opt, *log_level=NULL, *log_filename=NULL;
  struct epoll_event ev, events[MAX_EVENTS];

  nl = sizeof(left_blocks) / sizeof(block_def);
  nr = sizeof(right_blocks) / sizeof(block_def);

  block_module left_modules[nl];
  block_module right_modules[nr];

  block_module *module_map[BLOCK_COUNT];


  /* Parse options */
  
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

   /* Logging setup */

  log_init(log_level, log_filename);

  /* Start lemonbar */
  
  init_bar();
  
  /* Setup epoll */
  
  if ((epoll_fd = epoll_create1(0)) == -1) {
    perror("epoll_create1");
    exit(EXIT_FAILURE);
  }
  
  ev.events = EPOLLIN;
  ev.data.fd = bar_pipes[0];

  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, bar_pipes[0], &ev) == -1) {
    perror("epoll_ctl: add bar out pipe");
    exit(EXIT_FAILURE);
  }

  /* Block inits */

  for (i = 0; i < nl; i++){
    init_block(&left_blocks[i], &left_modules[i], epoll_fd);
    module_map[left_blocks[i].id] = &left_modules[i];
  }
  for (i = 0; i < nr; i++){
    init_block(&right_blocks[i], &right_modules[i], epoll_fd);
    module_map[right_blocks[i].id] = &right_modules[i];
  }


  /* Main Loop */
  
  while (1) {
	
    /* Wait for update */

	nfds = epoll_pwait(epoll_fd, events, MAX_EVENTS, -1, &sig_mask);
    if (nfds == -1){
      /* Signal interrupt */
      if (errno == EINTR) continue;
      perror("epoll_wait");
      exit(EXIT_FAILURE);
    }

    for (i = 0; i < nfds; i++) {

	  /* Mouse command from bar */

	  if (events[i].data.fd == bar_pipes[0]) {
		nb = read(events[i].data.fd, buf, BUF_SIZE);
		if (nb < 0) {
		  perror("read bytess");
		  exit(EXIT_FAILURE);
		}
		buf[strcspn(buf, "\r\n")] = '\0';
		process_command(buf);
		continue;
      }

      /* Block input */
	  
      if (read(events[i].data.fd, &id, sizeof(id)) == -1) {
      	perror("read bytes");
		exit(EXIT_FAILURE);
      }

	  log_info("Got message from %d", id);

      for (j = 0; j < module_map[id]->nt; j++) {
        read(events[i].data.fd, &module_map[id]->text[j], sizeof(block_text));
        log_info("%d-Received: \"%s\"", id, &module_map[id]->text[j].text);
      }
    }

	log_info("Writing");

	/* Write to bar */

    write(bar_pipes[3], "%{l}", 4);
    for (i = 0; i < nl; i++) {
      write_module(&left_modules[i], bar_pipes[3]);
    }

    
    write(bar_pipes[3], "%{r}", 4);
    for (i = 0; i < nr; i++) {
      write_module(&right_modules[i], bar_pipes[3]);
	  if (i != nr - 1) write(bar_pipes[3], " | ", 3);
    }
    
    /* Write to bar_pipes[3] */
    write(bar_pipes[3], "\n", 1);
  }
  
  pclose(bar_fd);
  log_destroy();
  return 0;
}
