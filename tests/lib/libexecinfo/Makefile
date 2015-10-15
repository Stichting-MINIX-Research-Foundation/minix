# $NetBSD: Makefile,v 1.6 2015/01/31 20:55:43 nakayama Exp $

.include <bsd.own.mk>

.if defined(__MINIX)
USE_BITCODE:=	no # LSC: can't use bitcode for this.
.endif # defined(__MINIX)

TESTSDIR=	${TESTSBASE}/lib/libexecinfo

TESTS_C+=	t_backtrace
STRIPFLAG=

LDADD+=		-lexecinfo -lelf
DPADD+=		${LIBEXECINFO} ${LIBELF}

COPTS.t_backtrace.c=	-Wno-stack-protector

.include <bsd.test.mk>
