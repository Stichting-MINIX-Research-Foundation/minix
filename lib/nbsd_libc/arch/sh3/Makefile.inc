#	$NetBSD: Makefile.inc,v 1.6 2006/06/17 18:04:23 uwe Exp $

SRCS+=	__sigaction14_sigtramp.c __sigtramp2.S

CPPFLAGS+=      -DSOFTFLOAT # -DSOFTFLOAT_NEED_FIXUNS
.include <softfloat/Makefile.inc>
