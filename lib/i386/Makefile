#Generated from ./i386/Makefile.in
all: all-ack

all-ack:
all-gnu:

makefiles: Makefile

Makefile: Makefile.in
	sh ../generate.sh ./i386 ../obj-ack ../obj-gnu
	@echo
	@echo "*Attention*"
	@echo "Makefile is regenerated... rerun command to see changes"
	@echo "*Attention*"
	@echo

all-ack: makefiles
	mkdir -p ../obj-ack/./i386/int64
	cd int64 && $(MAKE) $@
	mkdir -p ../obj-ack/./i386/misc
	cd misc && $(MAKE) $@
	mkdir -p ../obj-ack/./i386/rts
	cd rts && $(MAKE) $@
	mkdir -p ../obj-ack/./i386/string
	cd string && $(MAKE) $@

all-gnu: makefiles
	mkdir -p ../obj-gnu/./i386/int64
	cd int64 && $(MAKE) $@
	mkdir -p ../obj-gnu/./i386/misc
	cd misc && $(MAKE) $@
	mkdir -p ../obj-gnu/./i386/rts
	cd rts && $(MAKE) $@
	mkdir -p ../obj-gnu/./i386/string
	cd string && $(MAKE) $@

clean depend depend-ack depend-gnu:: makefiles
	cd int64 && $(MAKE) $@
	cd misc && $(MAKE) $@
	cd rts && $(MAKE) $@
	cd string && $(MAKE) $@

makefiles: int64/Makefile
makefiles: misc/Makefile
makefiles: rts/Makefile
makefiles: string/Makefile

int64/Makefile: int64/Makefile.in
	cd int64 && sh ../../generate.sh ./i386/int64 ../../obj-ack ../../obj-gnu && $(MAKE) makefiles
misc/Makefile: misc/Makefile.in
	cd misc && sh ../../generate.sh ./i386/misc ../../obj-ack ../../obj-gnu && $(MAKE) makefiles
rts/Makefile: rts/Makefile.in
	cd rts && sh ../../generate.sh ./i386/rts ../../obj-ack ../../obj-gnu && $(MAKE) makefiles
string/Makefile: string/Makefile.in
	cd string && sh ../../generate.sh ./i386/string ../../obj-ack ../../obj-gnu && $(MAKE) makefiles

clean::
	rm -f ../obj-ack/./i386/*
	rm -f ../obj-gnu/./i386/*

include Makedepend-ack
include .depend-ack

include Makedepend-gnu
include .depend-gnu
