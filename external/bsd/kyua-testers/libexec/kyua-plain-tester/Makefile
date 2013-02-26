# $NetBSD: Makefile,v 1.1 2013/02/19 06:04:43 jmmv Exp $

.include <bsd.own.mk>

SRCDIR=		${NETBSDSRCDIR}/external/bsd/kyua-testers/dist
.PATH:		${SRCDIR}

BINDIR=		/usr/libexec
PROG_CXX=	kyua-plain-tester
SRCS=		plain_main.c
MAN=		kyua-plain-tester.1
MAN+=		kyua-plain-interface.7

PRIVATELIBDIR!=	cd ${.CURDIR}/../../lib; ${PRINTOBJDIR}
LDADD+=		${PRIVATELIBDIR}/libtester/libtester.a
DPADD+=		${PRIVATELIBDIR}/libtester/libtester.a

CPPFLAGS+=	-DHAVE_CONFIG_H
CPPFLAGS+=	-I${.CURDIR}/../../lib/libtester
CPPFLAGS+=	-I${SRCDIR}

WARNS?=		3

CLEANFILES+=	kyua-plain-tester.1
kyua-plain-tester.1:	kyua-plain-tester.1.in
	sed -e 's,__PACKAGE__,kyua-testers,g' \
	    -e 's,__TMPDIR__,/tmp,g' \
	    <${.ALLSRC} >${.TARGET}.tmp
	mv ${.TARGET}.tmp ${.TARGET}

.include <bsd.prog.mk>
