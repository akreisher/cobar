CC=gcc

.PHONY: debug cobar


all: cobar

cobar: cobar.c modules.o bspwm.o config.h
	$(CC) $(CFLAGS) -pthread -o $@ $^

debug: cobar.c modules.o config.h
	$(CC) $(CFLAGS) -g -DDEBUG -pthread -o cobar $^

test: test.c modules.o config.h
	$(CC) $(CFLAGS) -pthread -o $@ $^

modules.o: modules.c modules.h
	$(CC) $(CFLAGS) -c -o $@ $<

bspwm.o: bspwm.c modules.h
	$(CC) $(CFLAGS) -c -o $@ $<

install: all
	cp -f cobar ~/.local/bin/cobar

clean:
	rm cobar *.o
