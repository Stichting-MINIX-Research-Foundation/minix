#Generated from ./ack/math/Makefile.in
all: all-ack

all-ack:
all-gnu:

makefiles: Makefile

Makefile: Makefile.in
	sh ../../generate.sh ./ack/math ../../obj-ack ../../obj-gnu
	@echo
	@echo "*Attention*"
	@echo "Makefile is regenerated... rerun command to see changes"
	@echo "*Attention*"
	@echo

depend: depend-ack
all-ack: ../../obj-ack/libc.a

../../obj-ack/libc.a: ../../obj-ack/libc.a(frexp.o)
../../obj-ack/libc.a: ../../obj-ack/libc.a(modf.o)
../../obj-ack/libc.a: ../../obj-ack/libc.a(isnan.o)
../../obj-ack/libc.a: ../../obj-ack/libc.a(ldexp.o)

../../obj-ack/libc.a:
	ar cr ../../obj-ack/libc.a ../../obj-ack/./ack/math/*.o
	rm ../../obj-ack/./ack/math/*.o

../../obj-ack/libc.a(frexp.o): frexp.s
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../../obj-ack/./ack/math/frexp.o frexp.s
../../obj-ack/libc.a(modf.o): modf.s
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../../obj-ack/./ack/math/modf.o modf.s
../../obj-ack/libc.a(isnan.o): isnan.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../../obj-ack/./ack/math/isnan.o isnan.c
../../obj-ack/libc.a(ldexp.o): ldexp.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../../obj-ack/./ack/math/ldexp.o ldexp.c



clean::
	rm -f ../../obj-ack/./ack/math/*

include Makedepend-ack
include .depend-ack

include Makedepend-gnu
include .depend-gnu
