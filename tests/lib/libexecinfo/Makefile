# $NetBSD: Makefile,v 1.4 2013/07/21 15:29:04 tron Exp $

.include <bsd.own.mk>

.if defined(__MINIX)
USE_BITCODE:=	no # LSC: can't use bitcode for this.
.endif # defined(__MINIX)

TESTSDIR=	${TESTSBASE}/lib/libexecinfo

TESTS_C+=	t_backtrace

LDADD+=		-lexecinfo -lelf
DPADD+=		${LIBEXECINFO} ${LIBELF}

COPTS.t_backtrace.c=	-Wno-stack-protector

.include <bsd.test.mk>
