# Makefile for the init program (INIT)
.include <bsd.own.mk>

PROG=	init
SRCS=	init.c

MAN=

BINDIR?= /usr/sbin

LDFLAGS+= -Wl,--section-start=.init=0x0

.include <bsd.prog.mk>
