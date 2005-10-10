
CC=gcc
AR=gar
AS=gas
LD=gld
NM=gnm

# Makefile for lib/dummy.

# Make a dummy libm library so that -lm works.

all:	../libm.a

../libm.a:
	echo "int __dummy__;" >dummy.c
	$(CC) -c dummy.c
	$(AR) cr $@ *.o
	ranlib $@
	rm dummy.?
