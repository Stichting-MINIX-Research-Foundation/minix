#Generated from ./util/Makefile.in
all: all-ack

all-ack:
all-gnu:

makefiles: Makefile

Makefile: Makefile.in
	sh ../generate.sh ./util ../obj-ack ../obj-gnu
	@echo
	@echo "*Attention*"
	@echo "Makefile is regenerated... rerun command to see changes"
	@echo "*Attention*"
	@echo

depend: depend-ack
all-ack: ../obj-ack/libutil.a

../obj-ack/libutil.a: ../obj-ack/libutil.a(openpty.o)

../obj-ack/libutil.a:
	ar cr ../obj-ack/libutil.a ../obj-ack/./util/*.o
	rm ../obj-ack/./util/*.o

../obj-ack/libutil.a(openpty.o): openpty.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./util/openpty.o openpty.c

all-gnu: ../obj-gnu/libutil.a

../obj-gnu/libutil.a: ../obj-gnu/./util/openpty.o

../obj-gnu/libutil.a:
	gar cr ../obj-gnu/libutil.a $?

../obj-gnu/./util/openpty.o: openpty.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./util/openpty.o openpty.c




clean::
	rm -f ../obj-ack/./util/*
	rm -f ../obj-gnu/./util/*

include Makedepend-ack
include .depend-ack

include Makedepend-gnu
include .depend-gnu
