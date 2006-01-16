#Generated from ./regex/Makefile.in
all: all-ack

all-ack:
all-gnu:

makefiles: Makefile

Makefile: Makefile.in
	sh ../generate.sh ./regex ../obj-ack ../obj-gnu
	@echo
	@echo "*Attention*"
	@echo "Makefile is regenerated... rerun command to see changes"
	@echo "*Attention*"
	@echo

depend: depend-ack
all-ack: ../obj-ack/libc.a

../obj-ack/libc.a: ../obj-ack/libc.a(regcomp.o)
../obj-ack/libc.a: ../obj-ack/libc.a(regerror.o)
../obj-ack/libc.a: ../obj-ack/libc.a(regexec.o)
../obj-ack/libc.a: ../obj-ack/libc.a(regfree.o)

../obj-ack/libc.a:
	ar cr ../obj-ack/libc.a ../obj-ack/./regex/*.o
	rm ../obj-ack/./regex/*.o

../obj-ack/libc.a(regcomp.o): regcomp.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./regex/regcomp.o regcomp.c
../obj-ack/libc.a(regerror.o): regerror.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./regex/regerror.o regerror.c
../obj-ack/libc.a(regexec.o): regexec.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./regex/regexec.o regexec.c
../obj-ack/libc.a(regfree.o): regfree.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./regex/regfree.o regfree.c

all-gnu: ../obj-gnu/libc.a

../obj-gnu/libc.a: ../obj-gnu/./regex/regcomp.o
../obj-gnu/libc.a: ../obj-gnu/./regex/regerror.o
../obj-gnu/libc.a: ../obj-gnu/./regex/regexec.o
../obj-gnu/libc.a: ../obj-gnu/./regex/regfree.o

../obj-gnu/libc.a:
	gar cr ../obj-gnu/libc.a $?

../obj-gnu/./regex/regcomp.o: regcomp.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./regex/regcomp.o regcomp.c

../obj-gnu/./regex/regerror.o: regerror.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./regex/regerror.o regerror.c

../obj-gnu/./regex/regexec.o: regexec.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./regex/regexec.o regexec.c

../obj-gnu/./regex/regfree.o: regfree.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./regex/regfree.o regfree.c




clean::
	rm -f ../obj-ack/./regex/*
	rm -f ../obj-gnu/./regex/*

include Makedepend-ack
include .depend-ack

include Makedepend-gnu
include .depend-gnu
