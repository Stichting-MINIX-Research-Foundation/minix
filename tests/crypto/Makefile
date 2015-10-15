# $NetBSD: Makefile,v 1.5 2014/01/15 10:19:55 martin Exp $

.include <bsd.own.mk>

TESTS_SUBDIRS=		libcrypto
.if !defined(__MINIX)
TESTS_SUBDIRS+=		opencrypto
.endif # !defined(__MINIX)

TESTSDIR=	${TESTSBASE}/crypto

.include <bsd.test.mk>
