#Generated from ./ack/Makefile.in
all: all-ack

all-ack:
all-gnu:

makefiles: Makefile

Makefile: Makefile.in
	sh ../generate.sh ./ack ../obj-ack ../obj-gnu
	@echo
	@echo "*Attention*"
	@echo "Makefile is regenerated... rerun command to see changes"
	@echo "*Attention*"
	@echo

all-ack: makefiles
	mkdir -p ../obj-ack/./ack/float
	cd float && $(MAKE) $@
	mkdir -p ../obj-ack/./ack/fphook
	cd fphook && $(MAKE) $@
	mkdir -p ../obj-ack/./ack/i386
	cd i386 && $(MAKE) $@
	mkdir -p ../obj-ack/./ack/libm2
	cd libm2 && $(MAKE) $@
	mkdir -p ../obj-ack/./ack/libp
	cd libp && $(MAKE) $@
	mkdir -p ../obj-ack/./ack/liby
	cd liby && $(MAKE) $@
	mkdir -p ../obj-ack/./ack/math
	cd math && $(MAKE) $@
	mkdir -p ../obj-ack/./ack/rts
	cd rts && $(MAKE) $@

all-gnu: makefiles
	cd float && $(MAKE) $@
	cd fphook && $(MAKE) $@
	cd i386 && $(MAKE) $@
	cd libm2 && $(MAKE) $@
	cd libp && $(MAKE) $@
	cd liby && $(MAKE) $@
	cd math && $(MAKE) $@
	cd rts && $(MAKE) $@

clean depend depend-ack depend-gnu:: makefiles
	cd float && $(MAKE) $@
	cd fphook && $(MAKE) $@
	cd i386 && $(MAKE) $@
	cd libm2 && $(MAKE) $@
	cd libp && $(MAKE) $@
	cd liby && $(MAKE) $@
	cd math && $(MAKE) $@
	cd rts && $(MAKE) $@

makefiles: float/Makefile
makefiles: fphook/Makefile
makefiles: i386/Makefile
makefiles: libm2/Makefile
makefiles: libp/Makefile
makefiles: liby/Makefile
makefiles: math/Makefile
makefiles: rts/Makefile

float/Makefile: float/Makefile.in
	cd float && sh ../../generate.sh ./ack/float ../../obj-ack ../../obj-gnu && $(MAKE) makefiles
fphook/Makefile: fphook/Makefile.in
	cd fphook && sh ../../generate.sh ./ack/fphook ../../obj-ack ../../obj-gnu && $(MAKE) makefiles
i386/Makefile: i386/Makefile.in
	cd i386 && sh ../../generate.sh ./ack/i386 ../../obj-ack ../../obj-gnu && $(MAKE) makefiles
libm2/Makefile: libm2/Makefile.in
	cd libm2 && sh ../../generate.sh ./ack/libm2 ../../obj-ack ../../obj-gnu && $(MAKE) makefiles
libp/Makefile: libp/Makefile.in
	cd libp && sh ../../generate.sh ./ack/libp ../../obj-ack ../../obj-gnu && $(MAKE) makefiles
liby/Makefile: liby/Makefile.in
	cd liby && sh ../../generate.sh ./ack/liby ../../obj-ack ../../obj-gnu && $(MAKE) makefiles
math/Makefile: math/Makefile.in
	cd math && sh ../../generate.sh ./ack/math ../../obj-ack ../../obj-gnu && $(MAKE) makefiles
rts/Makefile: rts/Makefile.in
	cd rts && sh ../../generate.sh ./ack/rts ../../obj-ack ../../obj-gnu && $(MAKE) makefiles

clean::
	rm -f ../obj-ack/./ack/*

include Makedepend-ack
include .depend-ack

include Makedepend-gnu
include .depend-gnu
