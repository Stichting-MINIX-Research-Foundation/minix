#Generated from ./editline/Makefile.in
all: all-ack

all-ack:
all-gnu:

makefiles: Makefile

Makefile: Makefile.in
	sh ../generate.sh ./editline ../obj-ack ../obj-gnu
	@echo
	@echo "*Attention*"
	@echo "Makefile is regenerated... rerun command to see changes"
	@echo "*Attention*"
	@echo

depend: depend-ack
all-ack: ../obj-ack/libedit.a

../obj-ack/libedit.a: ../obj-ack/libedit.a(editline.o)
../obj-ack/libedit.a: ../obj-ack/libedit.a(complete.o)
../obj-ack/libedit.a: ../obj-ack/libedit.a(sysunix.o)

../obj-ack/libedit.a:
	ar cr ../obj-ack/libedit.a ../obj-ack/./editline/*.o
	rm ../obj-ack/./editline/*.o

../obj-ack/libedit.a(editline.o): editline.c
	cc -O -D_MINIX -D_POSIX_SOURCE -DANSI_ARROWS -DHAVE_STDLIB -DHAVE_TCGETATTR -DHIDE -DUSE_DIRENT 	-DHIST_SIZE=100 -DUSE_TERMCAP -DSYS_UNIX -wo -c -o ../obj-ack/./editline/editline.o editline.c
../obj-ack/libedit.a(complete.o): complete.c
	cc -O -D_MINIX -D_POSIX_SOURCE -DANSI_ARROWS -DHAVE_STDLIB -DHAVE_TCGETATTR -DHIDE -DUSE_DIRENT 	-DHIST_SIZE=100 -DUSE_TERMCAP -DSYS_UNIX -wo -c -o ../obj-ack/./editline/complete.o complete.c
../obj-ack/libedit.a(sysunix.o): sysunix.c
	cc -O -D_MINIX -D_POSIX_SOURCE -DANSI_ARROWS -DHAVE_STDLIB -DHAVE_TCGETATTR -DHIDE -DUSE_DIRENT 	-DHIST_SIZE=100 -DUSE_TERMCAP -DSYS_UNIX -wo -c -o ../obj-ack/./editline/sysunix.o sysunix.c

all-gnu: ../obj-gnu/libedit.a

../obj-gnu/libedit.a: ../obj-gnu/./editline/editline.o
../obj-gnu/libedit.a: ../obj-gnu/./editline/complete.o
../obj-gnu/libedit.a: ../obj-gnu/./editline/sysunix.o

../obj-gnu/libedit.a:
	gar cr ../obj-gnu/libedit.a $?

../obj-gnu/./editline/editline.o: editline.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -DANSI_ARROWS -DHAVE_STDLIB -DHAVE_TCGETATTR -DHIDE -DUSE_DIRENT 	-DHIST_SIZE=100 -DUSE_TERMCAP -DSYS_UNIX -wo -c -o ../obj-gnu/./editline/editline.o editline.c

../obj-gnu/./editline/complete.o: complete.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -DANSI_ARROWS -DHAVE_STDLIB -DHAVE_TCGETATTR -DHIDE -DUSE_DIRENT 	-DHIST_SIZE=100 -DUSE_TERMCAP -DSYS_UNIX -wo -c -o ../obj-gnu/./editline/complete.o complete.c

../obj-gnu/./editline/sysunix.o: sysunix.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -DANSI_ARROWS -DHAVE_STDLIB -DHAVE_TCGETATTR -DHIDE -DUSE_DIRENT 	-DHIST_SIZE=100 -DUSE_TERMCAP -DSYS_UNIX -wo -c -o ../obj-gnu/./editline/sysunix.o sysunix.c




clean::
	rm -f ../obj-ack/./editline/*
	rm -f ../obj-gnu/./editline/*

include Makedepend-ack
include .depend-ack

include Makedepend-gnu
include .depend-gnu
