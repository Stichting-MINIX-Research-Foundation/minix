#Generated from ./gnu/rts/Makefile.in
all: all-ack

all-ack:
all-gnu:

makefiles: Makefile

Makefile: Makefile.in
	sh ../../generate.sh ./gnu/rts ../../obj-ack ../../obj-gnu
	@echo
	@echo "*Attention*"
	@echo "Makefile is regenerated... rerun command to see changes"
	@echo "*Attention*"
	@echo

depend: depend-ack
all-gnu: ../../obj-gnu/libc.a

../../obj-gnu/libc.a: ../../obj-gnu/./gnu/rts/_longjmp.o
../../obj-gnu/libc.a: ../../obj-gnu/./gnu/rts/_setjmp.o
../../obj-gnu/libc.a: ../../obj-gnu/./gnu/rts/longjmp.o
../../obj-gnu/libc.a: ../../obj-gnu/./gnu/rts/setjmp.o

../../obj-gnu/libc.a:
	gar cr ../../obj-gnu/libc.a $?

../../obj-gnu/./gnu/rts/_longjmp.o: _longjmp.gs
	gas -o ../../obj-gnu/./gnu/rts/_longjmp.o _longjmp.gs

../../obj-gnu/./gnu/rts/_setjmp.o: _setjmp.gs
	gas -o ../../obj-gnu/./gnu/rts/_setjmp.o _setjmp.gs

../../obj-gnu/./gnu/rts/longjmp.o: longjmp.gs
	gas -o ../../obj-gnu/./gnu/rts/longjmp.o longjmp.gs

../../obj-gnu/./gnu/rts/setjmp.o: setjmp.gs
	gas -o ../../obj-gnu/./gnu/rts/setjmp.o setjmp.gs




clean::
	rm -f ../../obj-gnu/./gnu/rts/*

include Makedepend-ack
include .depend-ack

include Makedepend-gnu
include .depend-gnu
