# Makefile for the tests

CC = exec cc
CFLAGS = -Wall

PROG =	speed test00 test01 test02 test03 test04_srv test04_cli test05_srv \
        test05_cli test06_srv test06_cli test07_srv test07_cli test08_srv \
        test08_cli test09 test10 test11 test12 test13a test13b test14

all: $(PROG)

$(PROG): 
	$(CC) $(CFLAGS) -o $@ $@.c -lutil

clean:
	rm -f *.o $(PROG)

speed: speed.c
test00: test00.c
test01: test01.c
test02: test02.c
test03: test03.c
test04_cli: test04_cli.c
test04_srv: test04_srv.c
test05_cli: test05_cli.c
test05_srv: test05_srv.c
test06_cli: test06_cli.c
test06_srv: test06_srv.c
test07_cli: test07_cli.c
test07_srv: test07_srv.c
test08_cli: test08_cli.c
test08_srv: test08_srv.c
test09: test09.c
test10: test10.c
test11: test11.c
test12: test12.c
test13a: test13a.c
test13b: test13b.c
test14: test14.c
