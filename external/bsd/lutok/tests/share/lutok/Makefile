# $NetBSD: Makefile,v 1.1 2013/02/16 21:29:51 jmmv Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/share/examples/lutok

SRCDIR=		${NETBSDSRCDIR}/external/bsd/lutok/dist
.PATH:		${SRCDIR}

FILESDIR=	${TESTSDIR}

TESTS_SH=	examples_test
TESTS_SH_SRC_examples_test=	examples_test.patched

examples_test.patched: examples_test.sh
	${TOOL_SED} \
	    -e 's,__ATF_SH__,/bin/sh,g' \
	    -e 's,__EXAMPLESDIR__,/usr/share/examples/lutok,g' \
	    -e 's,__LIBDIR__,/usr/lib,g' \
	    <${SRCDIR}/examples_test.sh >examples_test.patched
CLEANFILES+=	examples_test.patched

.include <bsd.test.mk>
