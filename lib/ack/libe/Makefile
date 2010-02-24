# Makefile for libe

.include <minix.own.mk>

LIB=		e

AFLAGS+=-O -D_MINIX -D_POSIX_SOURCE -Was-ack

.include "${.CURDIR}/arch/${ARCH}/em/Makefile.inc"
.include "${.CURDIR}/arch/${ARCH}/head/Makefile.inc"

.include <minix.lib.mk>
