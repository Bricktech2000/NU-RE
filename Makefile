.POSIX:
.SUFFIXES:
CC=gcc
CFLAGS=-O2 -Wall -Wextra -Wpedantic -std=c99

all: test
clean:; rm -f test nu-re.o

test:    nu-re.o test.c;  $(CC) $(CFLAGS) -o $@ nu-re.o test.c -Wno-sign-compare
nu-re.o: nu-re.h nu-re.c; $(CC) $(CFLAGS) -o $@ -c nu-re.c -Wno-implicit-fallthrough -Wno-missing-field-initializers
