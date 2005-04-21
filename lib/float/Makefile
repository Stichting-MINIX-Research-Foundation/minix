# Makefile for lib/float.

CC1	= ./FP.compile

LIBRARY	= ../libfp.a
all:	$(LIBRARY)

OBJECTS	= \
	$(LIBRARY)(add_ext.o) \
	$(LIBRARY)(adder.o) \
	$(LIBRARY)(adf4.o) \
	$(LIBRARY)(adf8.o) \
	$(LIBRARY)(cff4.o) \
	$(LIBRARY)(cff8.o) \
	$(LIBRARY)(cfi.o) \
	$(LIBRARY)(cfu.o) \
	$(LIBRARY)(cif4.o) \
	$(LIBRARY)(cif8.o) \
	$(LIBRARY)(cmf4.o) \
	$(LIBRARY)(cmf8.o) \
	$(LIBRARY)(compact.o) \
	$(LIBRARY)(cuf4.o) \
	$(LIBRARY)(cuf8.o) \
	$(LIBRARY)(div_ext.o) \
	$(LIBRARY)(dvf4.o) \
	$(LIBRARY)(dvf8.o) \
	$(LIBRARY)(extend.o) \
	$(LIBRARY)(fef4.o) \
	$(LIBRARY)(fef8.o) \
	$(LIBRARY)(fif4.o) \
	$(LIBRARY)(fif8.o) \
	$(LIBRARY)(fptrp.o) \
	$(LIBRARY)(mlf4.o) \
	$(LIBRARY)(mlf8.o) \
	$(LIBRARY)(mul_ext.o) \
	$(LIBRARY)(ngf4.o) \
	$(LIBRARY)(ngf8.o) \
	$(LIBRARY)(nrm_ext.o) \
	$(LIBRARY)(sbf4.o) \
	$(LIBRARY)(sbf8.o) \
	$(LIBRARY)(sft_ext.o) \
	$(LIBRARY)(shifter.o) \
	$(LIBRARY)(sub_ext.o) \
	$(LIBRARY)(zrf4.o) \
	$(LIBRARY)(zrf8.o) \
	$(LIBRARY)(zrf_ext.o) \

$(LIBRARY):	$(OBJECTS)
	aal cr $@ *.o
	rm *.o

$(LIBRARY)(add_ext.o):	add_ext.c
	$(CC1) add_ext.c

$(LIBRARY)(adder.o):	adder.c
	$(CC1) adder.c

$(LIBRARY)(adf4.o):	adf4.c
	$(CC1) adf4.c

$(LIBRARY)(adf8.o):	adf8.c
	$(CC1) adf8.c

$(LIBRARY)(cff4.o):	cff4.c
	$(CC1) cff4.c

$(LIBRARY)(cff8.o):	cff8.c
	$(CC1) cff8.c

$(LIBRARY)(cfi.o):	cfi.c
	$(CC1) cfi.c

$(LIBRARY)(cfu.o):	cfu.c
	$(CC1) cfu.c

$(LIBRARY)(cif4.o):	cif4.c
	$(CC1) cif4.c

$(LIBRARY)(cif8.o):	cif8.c
	$(CC1) cif8.c

$(LIBRARY)(cmf4.o):	cmf4.c
	$(CC1) cmf4.c

$(LIBRARY)(cmf8.o):	cmf8.c
	$(CC1) cmf8.c

$(LIBRARY)(compact.o):	compact.c
	$(CC1) compact.c

$(LIBRARY)(cuf4.o):	cuf4.c
	$(CC1) cuf4.c

$(LIBRARY)(cuf8.o):	cuf8.c
	$(CC1) cuf8.c

$(LIBRARY)(div_ext.o):	div_ext.c
	$(CC1) div_ext.c

$(LIBRARY)(dvf4.o):	dvf4.c
	$(CC1) dvf4.c

$(LIBRARY)(dvf8.o):	dvf8.c
	$(CC1) dvf8.c

$(LIBRARY)(extend.o):	extend.c
	$(CC1) extend.c

$(LIBRARY)(fef4.o):	fef4.c
	$(CC1) fef4.c

$(LIBRARY)(fef8.o):	fef8.c
	$(CC1) fef8.c

$(LIBRARY)(fif4.o):	fif4.c
	$(CC1) fif4.c

$(LIBRARY)(fif8.o):	fif8.c
	$(CC1) fif8.c

$(LIBRARY)(fptrp.o):	fptrp.s
	$(CC) -c fptrp.s

$(LIBRARY)(mlf4.o):	mlf4.c
	$(CC1) mlf4.c

$(LIBRARY)(mlf8.o):	mlf8.c
	$(CC1) mlf8.c

$(LIBRARY)(mul_ext.o):	mul_ext.c
	$(CC1) mul_ext.c

$(LIBRARY)(ngf4.o):	ngf4.c
	$(CC1) ngf4.c

$(LIBRARY)(ngf8.o):	ngf8.c
	$(CC1) ngf8.c

$(LIBRARY)(nrm_ext.o):	nrm_ext.c
	$(CC1) nrm_ext.c

$(LIBRARY)(sbf4.o):	sbf4.c
	$(CC1) sbf4.c

$(LIBRARY)(sbf8.o):	sbf8.c
	$(CC1) sbf8.c

$(LIBRARY)(sft_ext.o):	sft_ext.c
	$(CC1) sft_ext.c

$(LIBRARY)(shifter.o):	shifter.c
	$(CC1) shifter.c

$(LIBRARY)(sub_ext.o):	sub_ext.c
	$(CC1) sub_ext.c

$(LIBRARY)(zrf4.o):	zrf4.c
	$(CC1) zrf4.c

$(LIBRARY)(zrf8.o):	zrf8.c
	$(CC1) zrf8.c

$(LIBRARY)(zrf_ext.o):	zrf_ext.c
	$(CC1) zrf_ext.c
