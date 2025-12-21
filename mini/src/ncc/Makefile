CFLAGS=-g -Wall

all: ncc libncp.a

ncc: ncc.o imp.o
	$(CC) -o $@ $^

libncp.a: libncp.o
	ar rcs $@ $^
	ranlib $@
