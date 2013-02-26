# $NetBSD: Makefile,v 1.46 2013/01/27 02:32:39 christos Exp $

.include "Makefile.inc"
.include <bsd.own.mk>

SUBDIR+=	tls_dso .WAIT

# LSC: Unsupported on MINIX
# c063 gen rpc setjmp 
# sys tls ttyio
TESTS_SUBDIRS+=	db hash inet locale net regex stdlib
TESTS_SUBDIRS+=	stdio string termios time

.if !defined(__MINIX)
.if ${HAVE_SSP} == "yes"
TESTS_SUBDIRS+=	ssp
.endif
.endif # !defined(__MINIX)

TESTSDIR=	${TESTSBASE}/lib/libc

TESTS_C+=	t_convfp
TESTS_C+=	t_gdtoa
TESTS_C+=	t_cdb

.include <bsd.test.mk>
