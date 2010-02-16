# ieee_float sources
.PATH: ${.CURDIR}/gnu/ieee_float

CPPFLAGS.fphook.c=-I${.CURDIR}/stdio

SRCS+=  \
	frexp.c \
	isinf.c \
	isnan.c \
	ldexp.c \
	modf.c \
	fphook.c
