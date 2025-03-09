CC=gcc
CFLAGS=-O2 -Wall -Wextra -Wpedantic -std=c99

all: bin/test

bin/test: test.c bin/nu-re.o
	mkdir -p bin/
	$(CC) $(CFLAGS) -Wno-sign-compare $^ -o $@

bin/nu-re.o: nu-re.c nu-re.h
	mkdir -p bin/
	$(CC) $(CFLAGS) -Wno-implicit-fallthrough -Wno-missing-field-initializers -c $< -o $@

clean:
	rm -rf bin/
