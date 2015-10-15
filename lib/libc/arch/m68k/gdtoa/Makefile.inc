#	$NetBSD: Makefile.inc,v 1.5 2014/03/18 18:20:37 riastradh Exp $

SRCS+=	strtof.c

.if ${MACHINE_ARCH} == "m68k"
SRCS+=	strtold_pxL.c
SRCS+=	strtopxL.c
.endif
