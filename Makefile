.POSIX:
.SUFFIXES:
CC=gcc
CFLAGS=-O2 -Wall -Wextra -Wpedantic -std=c99

all: bin/test
bin/:; mkdir bin/
clean:; rm -rf bin/

bin/test:    bin/ bin/nu-re.o test.c; $(CC) $(CFLAGS) -o $@ bin/nu-re.o test.c -Wno-sign-compare
bin/nu-re.o: bin/ nu-re.h nu-re.c;    $(CC) $(CFLAGS) -o $@ -c nu-re.c -Wno-implicit-fallthrough -Wno-missing-field-initializers
