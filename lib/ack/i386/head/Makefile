#Generated from ./ack/i386/head/Makefile.in
all: all-ack

all-ack:
all-gnu:

makefiles: Makefile

Makefile: Makefile.in
	sh ../../../generate.sh ./ack/i386/head ../../../obj-ack ../../../obj-gnu
	@echo
	@echo "*Attention*"
	@echo "Makefile is regenerated... rerun command to see changes"
	@echo "*Attention*"
	@echo

depend: depend-ack
all-ack: ../../../obj-ack/libe.a

../../../obj-ack/libe.a: ../../../obj-ack/libe.a(em_head.o)

../../../obj-ack/libe.a:
	ar cr ../../../obj-ack/libe.a ../../../obj-ack/./ack/i386/head/*.o
	rm ../../../obj-ack/./ack/i386/head/*.o

../../../obj-ack/libe.a(em_head.o): em_head.s
	cc  -c -o ../../../obj-ack/./ack/i386/head/em_head.o em_head.s



clean::
	rm -f ../../../obj-ack/./ack/i386/head/*

include Makedepend-ack
include .depend-ack

include Makedepend-gnu
include .depend-gnu
