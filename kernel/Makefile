# Makefile for kernel
.include <minix.own.mk>

PROG=	kernel

# first-stage, arch-dependent startup code
SRCS=	mpx.S
SRCS+=	start.c table.c main.c proc.c \
	system.c clock.c utility.c debug.c profile.c interrupt.c \
	watchdog.c

DPADD+=	${LIBTIMERS} ${LIBSYS}
LDADD+=	-ltimers -lsys

LDFLAGS+=	-.o
CPPFLAGS+=	-I${.CURDIR}/arch/${ARCH}/include -I${MINIXSRCDIR}
AFLAGS+=	-I${.CURDIR}/arch/${ARCH}/include -I${MINIXSRCDIR}

#Tell ASMCONV to prepend underscores to symbols
ASMCONVFLAGS+=	-u

INSTALLFLAGS+=	-S 0
BINDIR=	/usr/sbin
MAN=

.include "system/Makefile.inc"
.include "arch/${ARCH}/Makefile.inc"
.include <minix.prog.mk>
