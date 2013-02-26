# $NetBSD: Makefile,v 1.5 2013/02/14 21:07:25 martin Exp $

PROG=		sqlite3

SRCS=		shell.c

DPADD+=		${LIBSQLITE3} ${LIBEDIT} ${LIBTERIMINFO}
LDADD+=		-lsqlite3 -ledit -lterminfo

BINDIR=		/usr/bin

.include <bsd.prog.mk>
