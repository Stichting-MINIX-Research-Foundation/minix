# Makefile for the init program (INIT)
.include <bsd.own.mk>

PROG=	init
SRCS=	init.c

MAN=

BINDIR?= /usr/sbin
INSTALLFLAGS+=	-S 64k

LDFLAGS+= -Wl,--section-start=.init=0x0

.include <bsd.prog.mk>
