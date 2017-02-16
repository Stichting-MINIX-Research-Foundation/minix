# for building the dlz_example driver we don't use
# the bind9 build structure as the aim is to provide an
# example that is separable from the bind9 source tree

# this means this Makefile is not portable, so the testsuite
# skips this test on platforms where it doesn't build

CFLAGS=-Wall -fPIC -g

all: dlz_example.so

dlz_example.so: dlz_example.o
	$(CC) $(CFLAGS) -shared -o dlz_example.so dlz_example.o

clean:
	rm -f dlz_example.o dlz_example.so
