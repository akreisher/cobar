/*
 * Copyright (c) 2017 rxi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "log.h"

/* Cobar Specific */
static FILE *log_file = NULL;
static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

static void log_lock_fn(void *udata, int lock_q) {
  if (lock_q)
    pthread_mutex_lock((pthread_mutex_t *) udata);
  else
    pthread_mutex_unlock((pthread_mutex_t *) udata);
}

void log_init(const char *log_level, const char *log_filename) {
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

void log_destroy() {
  if (log_file) {
    log_lock_fn(&log_lock, 0);
    fclose(log_file);
    log_lock_fn(&log_lock, 1);
  }
}

/* Start original log.c */

static struct {
  void *udata;
  log_LockFn lock;
  FILE *fp;
  int level;
  int quiet;
} L;


static const char *level_names[] = {
  "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

#ifdef LOG_USE_COLOR
static const char *level_colors[] = {
  "\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m"
};
#endif


static void lock(void)   {
  if (L.lock) {
    L.lock(L.udata, 1);
  }
}


static void unlock(void) {
  if (L.lock) {
    L.lock(L.udata, 0);
  }
}


void log_set_udata(void *udata) {
  L.udata = udata;
}


void log_set_lock(log_LockFn fn) {
  L.lock = fn;
}


void log_set_fp(FILE *fp) {
  L.fp = fp;
}


void log_set_level(int level) {
  L.level = level;
}


void log_set_quiet(int enable) {
  L.quiet = enable ? 1 : 0;
}


void log_log(int level, const char *file, int line, const char *fmt, ...) {
  if (level < L.level) {
    return;
  }

  /* Acquire lock */
  lock();

  /* Get current time */
  time_t t = time(NULL);
  struct tm *lt = localtime(&t);

  /* Log to stderr */
  if (!L.quiet) {
    va_list args;
    char buf[16];
    buf[strftime(buf, sizeof(buf), "%H:%M:%S", lt)] = '\0';
#ifdef LOG_USE_COLOR
    fprintf(
      stderr, "%s %s%-5s\x1b[0m \x1b[90m%s:%d:\x1b[0m ",
      buf, level_colors[level], level_names[level], file, line);
#else
    fprintf(stderr, "%s %-5s %s:%d: ", buf, level_names[level], file, line);
#endif
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    fflush(stderr);
  }

  /* Log to file */
  if (L.fp) {
    va_list args;
    char buf[32];
    buf[strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", lt)] = '\0';
    fprintf(L.fp, "%s %-5s %s:%d: ", buf, level_names[level], file, line);
    va_start(args, fmt);
    vfprintf(L.fp, fmt, args);
    va_end(args);
    fprintf(L.fp, "\n");
    fflush(L.fp);
  }

  /* Release lock */
  unlock();
}
