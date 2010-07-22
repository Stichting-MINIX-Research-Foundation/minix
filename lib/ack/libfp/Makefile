# Makefile for libfp

# .fc build rule
.SUFFIXES:	.o .fc

.fc.o:
	${_MKTARGET_COMPILE}
	cp ${.IMPSRC} tmp.c
	${CC} -I${.CURDIR} ${CFLAGS} -S tmp.c
	sed -f ${.CURDIR}/FP.script tmp.s > ${.PREFIX}.s
	${CC} -I${.CURDIR} -c -o ${.TARGET} ${.PREFIX}.s
	rm  -f tmp.c tmp.s ${PREFIX}.s

LIB=	fp

SRCS=	 \
	add_ext.fc \
	adder.fc \
	adf4.fc \
	adf8.fc \
	cff4.fc \
	cff8.fc \
	cfi.fc \
	cfu.fc \
	cif4.fc \
	cif8.fc \
	cmf4.fc \
	cmf8.fc \
	compact.fc \
	cuf4.fc \
	cuf8.fc \
	div_ext.fc \
	dvf4.fc \
	dvf8.fc \
	extend.fc \
	fef4.fc \
	fef8.fc \
	fif4.fc \
	fif8.fc \
	fptrp.s \
	mlf4.fc \
	mlf8.fc \
	mul_ext.fc \
	ngf4.fc \
	ngf8.fc \
	nrm_ext.fc \
	sbf4.fc \
	sbf8.fc \
	sft_ext.fc \
	shifter.fc \
	sub_ext.fc \
	zrf4.fc \
	zrf8.fc \
	zrf_ext.fc

.include <bsd.lib.mk>
