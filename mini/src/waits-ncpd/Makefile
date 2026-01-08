CC=gcc
CFLAGS=-g -Wall -O2

waitsconnect: waitsconnect.o imp.o
	$(CC) -o $@ $^

waitsconnect.o: waitsconnect.c ncp.h imp.h
	$(CC) $(CFLAGS) -c waitsconnect.c

imp.o: imp.c imp.h
	$(CC) $(CFLAGS) -c imp.c

clean:
	rm -f waitsconnect *.o

.PHONY: clean
