# Makefile for the benchmarks.

all::
	chmod 755 run

all clean::
	for b in *bench*; do cd $$b && $(MAKE) $@; done
