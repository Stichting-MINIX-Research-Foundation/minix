#Generated from ./gnu/ieee_float/Makefile.in
all: all-ack

all-ack:
all-gnu:

makefiles: Makefile

Makefile: Makefile.in
	sh ../../generate.sh ./gnu/ieee_float ../../obj-ack ../../obj-gnu
	@echo
	@echo "*Attention*"
	@echo "Makefile is regenerated... rerun command to see changes"
	@echo "*Attention*"
	@echo

depend: depend-ack
all-gnu: ../../obj-gnu/libc.a

../../obj-gnu/libc.a: ../../obj-gnu/./gnu/ieee_float/frexp.o
../../obj-gnu/libc.a: ../../obj-gnu/./gnu/ieee_float/isinf.o
../../obj-gnu/libc.a: ../../obj-gnu/./gnu/ieee_float/isnan.o
../../obj-gnu/libc.a: ../../obj-gnu/./gnu/ieee_float/ldexp.o
../../obj-gnu/libc.a: ../../obj-gnu/./gnu/ieee_float/modf.o
../../obj-gnu/libc.a: ../../obj-gnu/./gnu/ieee_float/fphook.o

../../obj-gnu/libc.a:
	gar cr ../../obj-gnu/libc.a $?

../../obj-gnu/./gnu/ieee_float/frexp.o: frexp.c
	gcc -O -I.. -c -o ../../obj-gnu/./gnu/ieee_float/frexp.o frexp.c

../../obj-gnu/./gnu/ieee_float/isinf.o: isinf.c
	gcc -O -I.. -c -o ../../obj-gnu/./gnu/ieee_float/isinf.o isinf.c

../../obj-gnu/./gnu/ieee_float/isnan.o: isnan.c
	gcc -O -I.. -c -o ../../obj-gnu/./gnu/ieee_float/isnan.o isnan.c

../../obj-gnu/./gnu/ieee_float/ldexp.o: ldexp.c
	gcc -O -I.. -c -o ../../obj-gnu/./gnu/ieee_float/ldexp.o ldexp.c

../../obj-gnu/./gnu/ieee_float/modf.o: modf.c
	gcc -O -I.. -c -o ../../obj-gnu/./gnu/ieee_float/modf.o modf.c

../../obj-gnu/./gnu/ieee_float/fphook.o: fphook.c
	gcc -O -I.. -c -o ../../obj-gnu/./gnu/ieee_float/fphook.o fphook.c




clean::
	rm -f ../../obj-gnu/./gnu/ieee_float/*

include Makedepend-ack
include .depend-ack

include Makedepend-gnu
include .depend-gnu
