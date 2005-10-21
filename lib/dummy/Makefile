# Makefile for lib/dummy.

# Make a dummy libm library so that -lm works.


LIBRARIES=libm

libm_OBJECTS=dummy.o

include ../Makefile.inc

dummy.c:
	echo "int __dummy__;" > $@
