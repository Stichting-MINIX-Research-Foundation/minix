#	$NetBSD: Makefile.inc,v 1.13 2011/01/17 08:23:53 matt Exp $

SRCS+=		__sigaction14_sigtramp.c __sigtramp2.S

CPPFLAGS+=	-I.

.if defined(MKSOFTFLOAT) && (${MKSOFTFLOAT} != "no")
.include <softfloat/Makefile.inc>
.endif
