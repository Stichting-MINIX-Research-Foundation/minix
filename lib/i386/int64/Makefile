#Generated from ./i386/int64/Makefile.in
all: all-ack

all-ack:
all-gnu:

makefiles: Makefile

Makefile: Makefile.in
	sh ../../generate.sh ./i386/int64 ../../obj-ack ../../obj-gnu
	@echo
	@echo "*Attention*"
	@echo "Makefile is regenerated... rerun command to see changes"
	@echo "*Attention*"
	@echo

depend: depend-ack
all-ack: ../../obj-ack/libc.a

../../obj-ack/libc.a: ../../obj-ack/libc.a(add64.o)
../../obj-ack/libc.a: ../../obj-ack/libc.a(add64u.o)
../../obj-ack/libc.a: ../../obj-ack/libc.a(cmp64.o)
../../obj-ack/libc.a: ../../obj-ack/libc.a(cv64u.o)
../../obj-ack/libc.a: ../../obj-ack/libc.a(cvu64.o)
../../obj-ack/libc.a: ../../obj-ack/libc.a(diff64.o)
../../obj-ack/libc.a: ../../obj-ack/libc.a(div64u.o)
../../obj-ack/libc.a: ../../obj-ack/libc.a(ex64.o)
../../obj-ack/libc.a: ../../obj-ack/libc.a(make64.o)
../../obj-ack/libc.a: ../../obj-ack/libc.a(mul64u.o)
../../obj-ack/libc.a: ../../obj-ack/libc.a(sub64.o)
../../obj-ack/libc.a: ../../obj-ack/libc.a(sub64u.o)

../../obj-ack/libc.a:
	ar cr ../../obj-ack/libc.a ../../obj-ack/./i386/int64/*.o
	rm ../../obj-ack/./i386/int64/*.o

../../obj-ack/libc.a(add64.o): add64.s
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../../obj-ack/./i386/int64/add64.o add64.s
../../obj-ack/libc.a(add64u.o): add64u.s
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../../obj-ack/./i386/int64/add64u.o add64u.s
../../obj-ack/libc.a(cmp64.o): cmp64.s
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../../obj-ack/./i386/int64/cmp64.o cmp64.s
../../obj-ack/libc.a(cv64u.o): cv64u.s
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../../obj-ack/./i386/int64/cv64u.o cv64u.s
../../obj-ack/libc.a(cvu64.o): cvu64.s
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../../obj-ack/./i386/int64/cvu64.o cvu64.s
../../obj-ack/libc.a(diff64.o): diff64.s
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../../obj-ack/./i386/int64/diff64.o diff64.s
../../obj-ack/libc.a(div64u.o): div64u.s
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../../obj-ack/./i386/int64/div64u.o div64u.s
../../obj-ack/libc.a(ex64.o): ex64.s
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../../obj-ack/./i386/int64/ex64.o ex64.s
../../obj-ack/libc.a(make64.o): make64.s
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../../obj-ack/./i386/int64/make64.o make64.s
../../obj-ack/libc.a(mul64u.o): mul64u.s
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../../obj-ack/./i386/int64/mul64u.o mul64u.s
../../obj-ack/libc.a(sub64.o): sub64.s
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../../obj-ack/./i386/int64/sub64.o sub64.s
../../obj-ack/libc.a(sub64u.o): sub64u.s
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../../obj-ack/./i386/int64/sub64u.o sub64u.s

all-gnu: ../../obj-gnu/libc.a

../../obj-gnu/libc.a: ../../obj-gnu/./i386/int64/add64.o
../../obj-gnu/libc.a: ../../obj-gnu/./i386/int64/add64u.o
../../obj-gnu/libc.a: ../../obj-gnu/./i386/int64/cmp64.o
../../obj-gnu/libc.a: ../../obj-gnu/./i386/int64/cv64u.o
../../obj-gnu/libc.a: ../../obj-gnu/./i386/int64/cvu64.o
../../obj-gnu/libc.a: ../../obj-gnu/./i386/int64/diff64.o
../../obj-gnu/libc.a: ../../obj-gnu/./i386/int64/div64u.o
../../obj-gnu/libc.a: ../../obj-gnu/./i386/int64/ex64.o
../../obj-gnu/libc.a: ../../obj-gnu/./i386/int64/make64.o
../../obj-gnu/libc.a: ../../obj-gnu/./i386/int64/mul64u.o
../../obj-gnu/libc.a: ../../obj-gnu/./i386/int64/sub64.o
../../obj-gnu/libc.a: ../../obj-gnu/./i386/int64/sub64u.o

../../obj-gnu/libc.a:
	gar cr ../../obj-gnu/libc.a $?

../../obj-gnu/./i386/int64/add64.o: add64.s
	gcc -O -D_MINIX -D_POSIX_SOURCE -E -x assembler-with-cpp -I. add64.s | asmconv -mi386 ack gnu > ../../obj-gnu/./i386/int64/add64.s.gnu || true
	gas -o ../../obj-gnu/./i386/int64/add64.o ../../obj-gnu/./i386/int64/add64.s.gnu

../../obj-gnu/./i386/int64/add64u.o: add64u.s
	gcc -O -D_MINIX -D_POSIX_SOURCE -E -x assembler-with-cpp -I. add64u.s | asmconv -mi386 ack gnu > ../../obj-gnu/./i386/int64/add64u.s.gnu || true
	gas -o ../../obj-gnu/./i386/int64/add64u.o ../../obj-gnu/./i386/int64/add64u.s.gnu

../../obj-gnu/./i386/int64/cmp64.o: cmp64.s
	gcc -O -D_MINIX -D_POSIX_SOURCE -E -x assembler-with-cpp -I. cmp64.s | asmconv -mi386 ack gnu > ../../obj-gnu/./i386/int64/cmp64.s.gnu || true
	gas -o ../../obj-gnu/./i386/int64/cmp64.o ../../obj-gnu/./i386/int64/cmp64.s.gnu

../../obj-gnu/./i386/int64/cv64u.o: cv64u.s
	gcc -O -D_MINIX -D_POSIX_SOURCE -E -x assembler-with-cpp -I. cv64u.s | asmconv -mi386 ack gnu > ../../obj-gnu/./i386/int64/cv64u.s.gnu || true
	gas -o ../../obj-gnu/./i386/int64/cv64u.o ../../obj-gnu/./i386/int64/cv64u.s.gnu

../../obj-gnu/./i386/int64/cvu64.o: cvu64.s
	gcc -O -D_MINIX -D_POSIX_SOURCE -E -x assembler-with-cpp -I. cvu64.s | asmconv -mi386 ack gnu > ../../obj-gnu/./i386/int64/cvu64.s.gnu || true
	gas -o ../../obj-gnu/./i386/int64/cvu64.o ../../obj-gnu/./i386/int64/cvu64.s.gnu

../../obj-gnu/./i386/int64/diff64.o: diff64.s
	gcc -O -D_MINIX -D_POSIX_SOURCE -E -x assembler-with-cpp -I. diff64.s | asmconv -mi386 ack gnu > ../../obj-gnu/./i386/int64/diff64.s.gnu || true
	gas -o ../../obj-gnu/./i386/int64/diff64.o ../../obj-gnu/./i386/int64/diff64.s.gnu

../../obj-gnu/./i386/int64/div64u.o: div64u.s
	gcc -O -D_MINIX -D_POSIX_SOURCE -E -x assembler-with-cpp -I. div64u.s | asmconv -mi386 ack gnu > ../../obj-gnu/./i386/int64/div64u.s.gnu || true
	gas -o ../../obj-gnu/./i386/int64/div64u.o ../../obj-gnu/./i386/int64/div64u.s.gnu

../../obj-gnu/./i386/int64/ex64.o: ex64.s
	gcc -O -D_MINIX -D_POSIX_SOURCE -E -x assembler-with-cpp -I. ex64.s | asmconv -mi386 ack gnu > ../../obj-gnu/./i386/int64/ex64.s.gnu || true
	gas -o ../../obj-gnu/./i386/int64/ex64.o ../../obj-gnu/./i386/int64/ex64.s.gnu

../../obj-gnu/./i386/int64/make64.o: make64.s
	gcc -O -D_MINIX -D_POSIX_SOURCE -E -x assembler-with-cpp -I. make64.s | asmconv -mi386 ack gnu > ../../obj-gnu/./i386/int64/make64.s.gnu || true
	gas -o ../../obj-gnu/./i386/int64/make64.o ../../obj-gnu/./i386/int64/make64.s.gnu

../../obj-gnu/./i386/int64/mul64u.o: mul64u.s
	gcc -O -D_MINIX -D_POSIX_SOURCE -E -x assembler-with-cpp -I. mul64u.s | asmconv -mi386 ack gnu > ../../obj-gnu/./i386/int64/mul64u.s.gnu || true
	gas -o ../../obj-gnu/./i386/int64/mul64u.o ../../obj-gnu/./i386/int64/mul64u.s.gnu

../../obj-gnu/./i386/int64/sub64.o: sub64.s
	gcc -O -D_MINIX -D_POSIX_SOURCE -E -x assembler-with-cpp -I. sub64.s | asmconv -mi386 ack gnu > ../../obj-gnu/./i386/int64/sub64.s.gnu || true
	gas -o ../../obj-gnu/./i386/int64/sub64.o ../../obj-gnu/./i386/int64/sub64.s.gnu

../../obj-gnu/./i386/int64/sub64u.o: sub64u.s
	gcc -O -D_MINIX -D_POSIX_SOURCE -E -x assembler-with-cpp -I. sub64u.s | asmconv -mi386 ack gnu > ../../obj-gnu/./i386/int64/sub64u.s.gnu || true
	gas -o ../../obj-gnu/./i386/int64/sub64u.o ../../obj-gnu/./i386/int64/sub64u.s.gnu




clean::
	rm -f ../../obj-ack/./i386/int64/*
	rm -f ../../obj-gnu/./i386/int64/*

include Makedepend-ack
include .depend-ack

include Makedepend-gnu
include .depend-gnu
