# $NetBSD: Makefile,v 1.2 2013/10/18 23:41:40 jmmv Exp $

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
${name}: ${name}.in ${MANPAGE_DEPS}
	name=${name}; ${BUILD_MANPAGE}
.endfor

.include <bsd.prog.mk>
