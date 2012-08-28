# $NetBSD: Makefile.inc,v 1.9 2010/07/06 05:59:52 mrg Exp $

.include <bsd.own.mk>

.if !defined(__MINIX)
SRCS+=	__sigaction14_sigtramp.c __sigtramp2.S
.endif

CPPFLAGS += -DSOFTFLOAT

SOFTFLOAT_BITS=32
.include <softfloat/Makefile.inc>
