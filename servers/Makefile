# Makefile for all system servers.
#
MAKE = exec make -$(MAKEFLAGS)

usage:
	@echo "" >&2
	@echo "Makefile for all system servers." >&2
	@echo "Usage:" >&2
	@echo "	make build    # Compile all system servers locally" >&2
	@echo "	make image    # Compile servers in boot image" >&2
	@echo "	make clean    # Remove local compiler results" >&2
	@echo "	make install  # Install servers to /etc/servers/" >&2
	@echo "	                (requires root privileges)" >&2
	@echo "" >&2

build: all
all install depend clean:
	cd ./pm && $(MAKE) $@
	cd ./fs && $(MAKE) $@
	cd ./rs && $(MAKE) $@
	cd ./is && $(MAKE) $@
	cd ./init && $(MAKE) $@
	cd ./inet && $(MAKE) $@

image:
	cd ./pm && $(MAKE) EXTRA_OPTS=$(EXTRA_OPTS) build
	cd ./fs && $(MAKE) EXTRA_OPTS=$(EXTRA_OPTS) build
	cd ./rs && $(MAKE) EXTRA_OPTS=$(EXTRA_OPTS) build
	cd ./init && $(MAKE) EXTRA_OPTS=$(EXTRA_OPTS) build


