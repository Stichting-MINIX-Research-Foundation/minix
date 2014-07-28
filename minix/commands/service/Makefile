# Makefile for service
PROGS=	service printconfig
SRCS.service=service.c parse.c util.c
SRCS.printconfig=print.c parse.c util.c

BINDIR= /bin
MAN.service=	service.8
MAN.printconfig=

CPPFLAGS+=	-I${NETBSDSRCDIR}/minix

.include <bsd.prog.mk>
