#	$NetBSD: Makefile.inc,v 1.9 2009/12/14 01:07:41 matt Exp $

SRCS+=	__sigaction14_sigtramp.c __sigtramp2.S

CPPFLAGS+= -I.
CPPFLAGS.assym.h+=-D__LIBC12_SOURCE__

.if ${MKSOFTFLOAT} != "no"
.include <softfloat/Makefile.inc>
CPPFLAGS+= -DSOFTFLOAT_NEED_FIXUNS
.endif
