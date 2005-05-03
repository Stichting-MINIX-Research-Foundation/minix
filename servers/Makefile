# Makefile for all system servers.
#
MAKE = exec make -$(MAKEFLAGS)

usage:
	@echo "" >&2
	@echo "Makefile for all system servers." >&2
	@echo "Usage:" >&2
	@echo "	make build    # Compile all system servers locally" >&2
	@echo "	make clean    # Remove local compiler results" >&2
	@echo "	make install  # Install servers to /etc/servers/" >&2
	@echo "	                (requires root privileges)" >&2
	@echo "" >&2

build: all
all install clean:
	cd ./pm && $(MAKE) $@
	cd ./fs && $(MAKE) $@
	cd ./is && $(MAKE) $@
	cd ./init && $(MAKE) $@
	cd ./inet && $(MAKE) $@

