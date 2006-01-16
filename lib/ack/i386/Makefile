#Generated from ./ack/i386/Makefile.in
all: all-ack

all-ack:
all-gnu:

makefiles: Makefile

Makefile: Makefile.in
	sh ../../generate.sh ./ack/i386 ../../obj-ack ../../obj-gnu
	@echo
	@echo "*Attention*"
	@echo "Makefile is regenerated... rerun command to see changes"
	@echo "*Attention*"
	@echo

all-ack: makefiles
	mkdir -p ../../obj-ack/./ack/i386/em
	cd em && $(MAKE) $@
	mkdir -p ../../obj-ack/./ack/i386/head
	cd head && $(MAKE) $@

all-gnu: makefiles
	cd em && $(MAKE) $@
	cd head && $(MAKE) $@

clean depend depend-ack depend-gnu:: makefiles
	cd em && $(MAKE) $@
	cd head && $(MAKE) $@

makefiles: em/Makefile
makefiles: head/Makefile

em/Makefile: em/Makefile.in
	cd em && sh ../../../generate.sh ./ack/i386/em ../../../obj-ack ../../../obj-gnu && $(MAKE) makefiles
head/Makefile: head/Makefile.in
	cd head && sh ../../../generate.sh ./ack/i386/head ../../../obj-ack ../../../obj-gnu && $(MAKE) makefiles

clean::
	rm -f ../../obj-ack/./ack/i386/*

include Makedepend-ack
include .depend-ack

include Makedepend-gnu
include .depend-gnu
