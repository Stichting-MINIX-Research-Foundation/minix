# Makefile for all device drivers.
#
MAKE = exec make -$(MAKEFLAGS)

usage:
	@echo "" >&2
	@echo "Makefile for all device drivers." >&2
	@echo "Usage:" >&2
	@echo "	make build    # Compile all device drivers locally" >&2
	@echo "	make clean    # Remove local compiler results" >&2
	@echo "	make install  # Install drivers to /etc/drivers/" >&2
	@echo "	                (requires root privileges)" >&2
	@echo "" >&2

build: all
all install depend clean:
	cd ./libdriver && $(MAKE) $@
	cd ./libpci && $(MAKE) $@
	cd ./tty && $(MAKE) $@
	cd ./memory && $(MAKE) $@
	cd ./at_wini && $(MAKE) $@
	cd ./floppy && $(MAKE) $@
	cd ./printer && $(MAKE) $@
	cd ./rtl8139 && $(MAKE) $@
	cd ./fxp && $(MAKE) $@
	cd ./dpeth && $(MAKE) $@
	cd ./log && $(MAKE) $@
	cd ./bios_wini && $(MAKE) $@
	cd ./random && $(MAKE) $@
