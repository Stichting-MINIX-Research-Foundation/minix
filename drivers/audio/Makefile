# Makefile for all audio drivers.
MAKE = exec make -$(MAKEFLAGS)

build: all

all install depend clean:
	cd ./es1370 && $(MAKE) $@
	cd ./es1371 && $(MAKE) $@

