#	$NetBSD: Makefile.inc,v 1.1 2013/07/18 21:29:31 matt Exp $

SRCS+=	modf.S
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
#
# These allow softfloat programs to use the FPU.
#
SRCS+=	adddf3.S addsf3.S cmpdf2.S cmpsf2.S divdf3.S divsf3.S \
	extendsfdf2.S fixdfsi.S fixunsdfsi.S fixunssfsi.S floatsidf.S \
	muldf3.S mulsf3.S subdf3.S subsf3.S truncdfsf2.S

SRCS+=	floatunsidf.S floatunsisf.S

SRCS+=	lesf2.S ltsf2.S nesf2.S unordsf2.S
SRCS+=	ledf2.S ltdf2.S nedf2.S unorddf2.S
