test:
	mkdir -p bin
	gcc -O2 -Wall -Wextra -Wpedantic -Wno-sign-compare -Wno-implicit-fallthrough -Wno-missing-field-initializers -std=c99 test.c nu-re.c -o bin/test

clean:
	rm -rf bin
