CFLAGS=-g -Wall

all: bbn-ncc libncp.a

bbn-ncc: ncp.o imp.o
	$(CC) -o $@ $^

libncp.a: libncp.o
	ar rcs $@ $^
	ranlib $@
