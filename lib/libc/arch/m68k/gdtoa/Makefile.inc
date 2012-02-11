#	$NetBSD: Makefile.inc,v 1.3 2006/03/21 22:50:13 he Exp $

SRCS+=	strtof.c

.if ${MACHINE_ARCH} != "m68000"
SRCS+=	strtold_pxL.c
SRCS+=	strtopxL.c
.endif
