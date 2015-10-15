# $NetBSD: Makefile,v 1.47 2014/02/17 10:10:41 martin Exp $

.include "Makefile.inc"
.include <bsd.own.mk>

# LSC: Unsupported on MINIX
SUBDIR+=	tls_dso .WAIT # sync

# LSC: Unsupported on MINIX
# c063 gen rpc setjmp 
# sys tls ttyio
# LSC: db hangs, so compiled, installed, but not added to the testsuite by
#      default
SUBDIR+= db
TESTS_SUBDIRS+=	hash inet locale net regex stdlib
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
