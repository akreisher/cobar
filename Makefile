CC=gcc

LOGDIR=log

.PHONY: debug cobar


all: cobar

cobar: cobar.c modules.o bspwm.o log.o config.h
	$(CC) $(CFLAGS) -pthread -o $@ $^

debug: cobar.c modules.o bspwm.o log.o config.h
	$(CC) $(CFLAGS) -g -DLOG_LEVEL=LOG_DEBUG -pthread -o cobar $^

test: test.c modules.o config.h
	$(CC) $(CFLAGS) -pthread -o $@ $^

modules.o: modules.c modules.h
	$(CC) $(CFLAGS) -c -o $@ $<

bspwm.o: bspwm.c modules.h
	$(CC) $(CFLAGS) -c -o $@ $<

log.o: $(LOGDIR)/log.c $(LOGDIR)/log.h
	$(CC) $(CFLAGS) -DLOG_USE_COLOR -c -o $@ $<

install: all
	cp -f cobar ~/.local/bin/cobar

clean:
	rm cobar *.o
