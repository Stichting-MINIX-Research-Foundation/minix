# $NetBSD: Makefile,v 1.4 2011/01/02 09:33:45 skrll Exp $

NOMAN=			# defined

.include <bsd.own.mk>

.if defined(__MINIX)
USE_BITCODE:=	no # LSC: can't use bitcode for this.
.endif # defined(__MINIX)

TESTSDIR=		${TESTSBASE}/lib/csu
TESTS_SH=		t_crt0
TESTS_SUBDIRS=
SUBDIR+=		dso

.include "Makefile.check_stack"

CPPFLAGS+=		${CPPFLAGS_CHECK_STACK}

BINDIR=			${TESTSDIR}
PROG_CXX=		h_initfini1 h_initfini2 h_initfini3
SRCS.h_initfini1=	h_initfini1.cxx h_initfini_common.cxx \
			${SRCS_CHECK_STACK}
SRCS.h_initfini2=	h_initfini1.cxx h_initfini_common.cxx \
			${SRCS_CHECK_STACK}
LDADD.h_initfini2+=	-static
SRCS.h_initfini3=	h_initfini3.cxx h_initfini_common.cxx \
			${SRCS_CHECK_STACK}
LDADD.h_initfini3+=	-Wl,-rpath,${TESTSDIR}

.include <bsd.test.mk>
