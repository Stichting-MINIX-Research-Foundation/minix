# Makefile for libexec

# LSC For now
NOGCCERROR:= yes 

LIB=	exec
INCS=	libexec.h
SRCS=	exec_aout.c exec_elf.c exec_general.c

.if (${NBSD_LIBC} != "no")
INCSDIR= /usr/include
.else
INCSDIR= /usr/include.ack
.endif

.include <bsd.lib.mk>
