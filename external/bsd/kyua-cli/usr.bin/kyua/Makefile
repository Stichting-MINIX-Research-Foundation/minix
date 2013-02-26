# $NetBSD: Makefile,v 1.1 2013/02/23 14:16:58 jmmv Exp $

KYUA_LIBS=	cli engine store engine utils

.include <bsd.init.mk>

.PATH:		${SRCDIR} ${SRCDIR}/doc

BINDIR=		${KYUA_BINDIR}
PROG_CXX=	kyua
SRCS=		main.cpp

WARNS?=		3

.for name in \
	kyua-about.1 kyua-config.1 kyua-db-exec.1 kyua-db-migrate.1 \
	kyua-debug.1 kyua-help.1 kyua-list.1 kyua-report-html.1 \
	kyua-report.1 kyua-test.1 kyua.1
MAN+=	${name}
${name}: ${name}.in
	name=${name}; ${BUILD_MANPAGE}
.endfor

.include <bsd.prog.mk>
