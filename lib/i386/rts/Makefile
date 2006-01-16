#Generated from ./i386/rts/Makefile.in
all: all-ack

all-ack:
all-gnu:

makefiles: Makefile

Makefile: Makefile.in
	sh ../../generate.sh ./i386/rts ../../obj-ack ../../obj-gnu
	@echo
	@echo "*Attention*"
	@echo "Makefile is regenerated... rerun command to see changes"
	@echo "*Attention*"
	@echo

depend: depend-ack
all-ack: ../../obj-ack/libc.a

../../obj-ack/libc.a: ../../obj-ack/libc.a(__sigreturn.o)
../../obj-ack/libc.a: ../../obj-ack/libc.a(_ipc.o)
../../obj-ack/libc.a: ../../obj-ack/libc.a(brksize.o)

../../obj-ack/libc.a:
	ar cr ../../obj-ack/libc.a ../../obj-ack/./i386/rts/*.o
	rm ../../obj-ack/./i386/rts/*.o

../../obj-ack/libc.a(__sigreturn.o): __sigreturn.s
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../../obj-ack/./i386/rts/__sigreturn.o __sigreturn.s
../../obj-ack/libc.a(_ipc.o): _ipc.s
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../../obj-ack/./i386/rts/_ipc.o _ipc.s
../../obj-ack/libc.a(brksize.o): brksize.s
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../../obj-ack/./i386/rts/brksize.o brksize.s

all-gnu: ../../obj-gnu/libc.a

../../obj-gnu/libc.a: ../../obj-gnu/./i386/rts/__sigreturn.o
../../obj-gnu/libc.a: ../../obj-gnu/./i386/rts/_ipc.o
../../obj-gnu/libc.a: ../../obj-gnu/./i386/rts/brksize.o

../../obj-gnu/libc.a:
	gar cr ../../obj-gnu/libc.a $?

../../obj-gnu/./i386/rts/__sigreturn.o: __sigreturn.s
	gcc -O -D_MINIX -D_POSIX_SOURCE -E -x assembler-with-cpp -I. __sigreturn.s | asmconv -mi386 ack gnu > ../../obj-gnu/./i386/rts/__sigreturn.s.gnu || true
	gas -o ../../obj-gnu/./i386/rts/__sigreturn.o ../../obj-gnu/./i386/rts/__sigreturn.s.gnu

../../obj-gnu/./i386/rts/_ipc.o: _ipc.s
	gcc -O -D_MINIX -D_POSIX_SOURCE -E -x assembler-with-cpp -I. _ipc.s | asmconv -mi386 ack gnu > ../../obj-gnu/./i386/rts/_ipc.s.gnu || true
	gas -o ../../obj-gnu/./i386/rts/_ipc.o ../../obj-gnu/./i386/rts/_ipc.s.gnu

../../obj-gnu/./i386/rts/brksize.o: brksize.s
	gcc -O -D_MINIX -D_POSIX_SOURCE -E -x assembler-with-cpp -I. brksize.s | asmconv -mi386 ack gnu > ../../obj-gnu/./i386/rts/brksize.s.gnu || true
	gas -o ../../obj-gnu/./i386/rts/brksize.o ../../obj-gnu/./i386/rts/brksize.s.gnu



all-ack: ../../obj-ack/crtso.o

../../obj-ack/crtso.o: crtso.s
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../../obj-ack/crtso.o crtso.s

all-gnu: ../../obj-gnu/crtso.o

../../obj-gnu/crtso.o: crtso.s
	gcc -O -D_MINIX -D_POSIX_SOURCE -E -x assembler-with-cpp -I. crtso.s | asmconv -mi386 ack gnu > ../../obj-gnu/./i386/rts/crtso.s.gnu || true
	gas -o ../../obj-gnu/crtso.o ../../obj-gnu/./i386/rts/crtso.s.gnu


all-ack: ../../obj-ack/m2rtso.o

../../obj-ack/m2rtso.o: m2rtso.s
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../../obj-ack/m2rtso.o m2rtso.s

all-gnu: ../../obj-gnu/m2rtso.o

../../obj-gnu/m2rtso.o: m2rtso.s
	gcc -O -D_MINIX -D_POSIX_SOURCE -E -x assembler-with-cpp -I. m2rtso.s | asmconv -mi386 ack gnu > ../../obj-gnu/./i386/rts/m2rtso.s.gnu || true
	gas -o ../../obj-gnu/m2rtso.o ../../obj-gnu/./i386/rts/m2rtso.s.gnu


all-ack: ../../obj-ack/prtso.o

../../obj-ack/prtso.o: prtso.s
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../../obj-ack/prtso.o prtso.s

all-gnu: ../../obj-gnu/prtso.o

../../obj-gnu/prtso.o: prtso.s
	gcc -O -D_MINIX -D_POSIX_SOURCE -E -x assembler-with-cpp -I. prtso.s | asmconv -mi386 ack gnu > ../../obj-gnu/./i386/rts/prtso.s.gnu || true
	gas -o ../../obj-gnu/prtso.o ../../obj-gnu/./i386/rts/prtso.s.gnu



clean::
	rm -f ../../obj-ack/./i386/rts/*
	rm -f ../../obj-gnu/./i386/rts/*

include Makedepend-ack
include .depend-ack

include Makedepend-gnu
include .depend-gnu
