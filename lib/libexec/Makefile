# Makefile for libexec

LIB=	exec
INCS=	libexec.h
SRCS=	exec_aout.c exec_elf.c

.if (${NBSD_LIBC} != "no")
INCSDIR= /usr/include
.else
INCSDIR= /usr/include.ack
.endif

.include <bsd.lib.mk>
