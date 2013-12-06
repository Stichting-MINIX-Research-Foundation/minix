#	$NetBSD: Makefile.inc,v 1.14 2013/08/30 21:28:59 matt Exp $

SRCS+=		__sigaction14_sigtramp.c __sigtramp2.S
SRCS+=		powerpc_initfini.c

CPPFLAGS+=	-I.

.if defined(MKSOFTFLOAT) && (${MKSOFTFLOAT} != "no")
.include <softfloat/Makefile.inc>
.endif
