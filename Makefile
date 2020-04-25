CFLAGS=-g


all: cobar

cobar: cobar.c modules.o config.h
	$(CC) $(CFLAGS) -pthread -o $@ $^

test: test.c modules.o config.h
	$(CC) $(CFLAGS) -pthread -o $@ $^

modules.o: modules.c modules.h
	$(CC) $(CFLAGS) -c -o $@ $<

install: all
	cp -f cobar ~/.local/bin/cobar

clean:
	rm cobar *.o
