#Generated from ./dummy/Makefile.in
all: all-ack

all-ack:
all-gnu:

makefiles: Makefile

Makefile: Makefile.in
	sh ../generate.sh ./dummy ../obj-ack ../obj-gnu
	@echo
	@echo "*Attention*"
	@echo "Makefile is regenerated... rerun command to see changes"
	@echo "*Attention*"
	@echo

depend: depend-ack
all-ack: ../obj-ack/libm.a

../obj-ack/libm.a: ../obj-ack/libm.a(dummy.o)

../obj-ack/libm.a:
	ar cr ../obj-ack/libm.a ../obj-ack/./dummy/*.o
	rm ../obj-ack/./dummy/*.o

../obj-ack/libm.a(dummy.o): dummy.c
	cc  -c -o ../obj-ack/./dummy/dummy.o dummy.c

all-gnu: ../obj-gnu/libm.a

../obj-gnu/libm.a: ../obj-gnu/./dummy/dummy.o

../obj-gnu/libm.a:
	gar cr ../obj-gnu/libm.a $?

../obj-gnu/./dummy/dummy.o: dummy.c
	gcc  -c -o ../obj-gnu/./dummy/dummy.o dummy.c




clean::
	rm -f ../obj-ack/./dummy/*
	rm -f ../obj-gnu/./dummy/*

include Makedepend-ack
include .depend-ack

include Makedepend-gnu
include .depend-gnu
