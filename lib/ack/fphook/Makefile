#Generated from ./ack/fphook/Makefile.in
all: all-ack

all-ack:
all-gnu:

makefiles: Makefile

Makefile: Makefile.in
	sh ../../generate.sh ./ack/fphook ../../obj-ack ../../obj-gnu
	@echo
	@echo "*Attention*"
	@echo "Makefile is regenerated... rerun command to see changes"
	@echo "*Attention*"
	@echo

depend: depend-ack
all-ack: ../../obj-ack/libd.a

../../obj-ack/libd.a: ../../obj-ack/libd.a(fphook.o)

../../obj-ack/libd.a:
	ar cr ../../obj-ack/libd.a ../../obj-ack/./ack/fphook/*.o
	rm ../../obj-ack/./ack/fphook/*.o

../../obj-ack/libd.a(fphook.o): fphook.c
	cc -O -D_MINIX -D_POSIX_SOURCE -I.. -c -o ../../obj-ack/./ack/fphook/fphook.o fphook.c

all-ack: ../../obj-ack/libc.a

../../obj-ack/libc.a: ../../obj-ack/libc.a(fltpr.o)
../../obj-ack/libc.a: ../../obj-ack/libc.a(strtod.o)

../../obj-ack/libc.a:
	ar cr ../../obj-ack/libc.a ../../obj-ack/./ack/fphook/*.o
	rm ../../obj-ack/./ack/fphook/*.o

../../obj-ack/libc.a(fltpr.o): fltpr.c
	cc -O -D_MINIX -D_POSIX_SOURCE -I.. -c -o ../../obj-ack/./ack/fphook/fltpr.o fltpr.c
../../obj-ack/libc.a(strtod.o): strtod.c
	cc -O -D_MINIX -D_POSIX_SOURCE -I.. -c -o ../../obj-ack/./ack/fphook/strtod.o strtod.c



clean::
	rm -f ../../obj-ack/./ack/fphook/*

include Makedepend-ack
include .depend-ack

include Makedepend-gnu
include .depend-gnu
