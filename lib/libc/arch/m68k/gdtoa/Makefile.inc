#	$NetBSD: Makefile.inc,v 1.4 2013/07/18 21:19:50 matt Exp $

SRCS+=	strtof.c

.if ${MACHINE_ARCH} == "m68k"
SRCS+=	strtold_pxL.c
SRCS+=	strtopxL.c
.endif
