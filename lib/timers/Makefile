#Generated from ./timers/Makefile.in
all: all-ack

all-ack:
all-gnu:

makefiles: Makefile

Makefile: Makefile.in
	sh ../generate.sh ./timers ../obj-ack ../obj-gnu
	@echo
	@echo "*Attention*"
	@echo "Makefile is regenerated... rerun command to see changes"
	@echo "*Attention*"
	@echo

depend: depend-ack
all-ack: ../obj-ack/libtimers.a

../obj-ack/libtimers.a: ../obj-ack/libtimers.a(tmrs_set.o)
../obj-ack/libtimers.a: ../obj-ack/libtimers.a(tmrs_clr.o)
../obj-ack/libtimers.a: ../obj-ack/libtimers.a(tmrs_exp.o)

../obj-ack/libtimers.a:
	ar cr ../obj-ack/libtimers.a ../obj-ack/./timers/*.o
	rm ../obj-ack/./timers/*.o

../obj-ack/libtimers.a(tmrs_set.o): tmrs_set.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./timers/tmrs_set.o tmrs_set.c
../obj-ack/libtimers.a(tmrs_clr.o): tmrs_clr.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./timers/tmrs_clr.o tmrs_clr.c
../obj-ack/libtimers.a(tmrs_exp.o): tmrs_exp.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./timers/tmrs_exp.o tmrs_exp.c

all-gnu: ../obj-gnu/libtimers.a

../obj-gnu/libtimers.a: ../obj-gnu/./timers/tmrs_set.o
../obj-gnu/libtimers.a: ../obj-gnu/./timers/tmrs_clr.o
../obj-gnu/libtimers.a: ../obj-gnu/./timers/tmrs_exp.o

../obj-gnu/libtimers.a:
	gar cr ../obj-gnu/libtimers.a $?

../obj-gnu/./timers/tmrs_set.o: tmrs_set.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./timers/tmrs_set.o tmrs_set.c

../obj-gnu/./timers/tmrs_clr.o: tmrs_clr.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./timers/tmrs_clr.o tmrs_clr.c

../obj-gnu/./timers/tmrs_exp.o: tmrs_exp.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./timers/tmrs_exp.o tmrs_exp.c




clean::
	rm -f ../obj-ack/./timers/*
	rm -f ../obj-gnu/./timers/*

include Makedepend-ack
include .depend-ack

include Makedepend-gnu
include .depend-gnu
