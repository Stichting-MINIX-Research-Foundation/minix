# Makefile for service
PROGS=	service printconfig
SRCS.service=service.c parse.c util.c
SRCS.printconfig=print.c parse.c util.c

BINDIR= /bin
MAN.service=
MAN.printconfig=

CPPFLAGS+=	-I${MINIXSRCDIR}

.include <bsd.prog.mk>
