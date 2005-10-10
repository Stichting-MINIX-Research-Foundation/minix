# Makefile for lib/float.

CC1	= /bin/sh ./FP.compile

LIBRARIES = libfp

libfp_OBJECTS	= \
	add_ext.o \
	adder.o \
	adf4.o \
	adf8.o \
	cff4.o \
	cff8.o \
	cfi.o \
	cfu.o \
	cif4.o \
	cif8.o \
	cmf4.o \
	cmf8.o \
	compact.o \
	cuf4.o \
	cuf8.o \
	div_ext.o \
	dvf4.o \
	dvf8.o \
	extend.o \
	fef4.o \
	fef8.o \
	fif4.o \
	fif8.o \
	fptrp.o \
	mlf4.o \
	mlf8.o \
	mul_ext.o \
	ngf4.o \
	ngf8.o \
	nrm_ext.o \
	sbf4.o \
	sbf8.o \
	sft_ext.o \
	shifter.o \
	sub_ext.o \
	zrf4.o \
	zrf8.o \
	zrf_ext.o \

include ../../Makefile.ack.inc

#extra commands to convert the c files to the correct assembler files

%.s: %.fc
	/bin/sh ./FP.compile $<

#1. make a assembler file of the c file
#%.fs: %.fc
#	-cp $< $(<:.fc=.c) && cc -O -I. -D_MINIX -D_POSIX_SOURCE -S $(<:.fc=.c) && cp $(<:.fc=.s) $(<:.fc=.fs)
#	@rm $(<:.fc=.c) $(<:.fc=.s)

#2. modify the assembler file
#%.s: %.fs
#	sed -f FP.script $< > $@
