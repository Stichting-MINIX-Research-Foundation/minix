#	$NetBSD: Makefile.inc,v 1.4 2011/01/20 18:41:39 matt Exp $

SRCS+=	strtof.c
.if (${MACHINE_ARCH} == "mips64eb" || ${MACHINE_ARCH} == "mips64el")
.if !defined(CPUFLAGS) || empty(CPUFLAGS:M-mabi=32)
SRCS+=	strtold_pQ.c
SRCS+=	strtopQ.c
.endif
.endif
