CC = gcc

CFLAGS := -lsensors -pthread

LOGDIR = log

USE_COLOR ?= -DLOG_USE_COLOR

.PHONY: debug cobar
.INTERMEDIATE: bspwm.o log.o modules.o

all: cobar

cobar: cobar.c modules.o bspwm.o log.o config.h
	$(CC) $(CFLAGS)  -o $@ $^

debug: cobar.c modules.o bspwm.o log.o config.h
	$(CC) $(CFLAGS) -g -DLOG_LEVEL=LOG_DEBUG -o cobar $^

test: test.c modules.o config.h
	$(CC) $(CFLAGS) -o $@ $^

modules.o: modules.c modules.h
	$(CC) $(CFLAGS) -c -o $@ $<

bspwm.o: bspwm.c modules.h
	$(CC) $(CFLAGS) -c -o $@ $<

log.o: $(LOGDIR)/log.c $(LOGDIR)/log.h
	$(CC) $(CFLAGS) $(USE_COLOR) -c -o $@ $<

install: all
	cp -f cobar ~/.local/bin/cobar

clean:
	rm cobar *.o
