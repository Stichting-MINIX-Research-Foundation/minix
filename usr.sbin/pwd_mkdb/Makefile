#	from: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$NetBSD: Makefile,v 1.20 2011/08/25 16:53:25 joerg Exp $

.include <bsd.own.mk>

PROG=	pwd_mkdb
MAN=	pwd_mkdb.8
CPPFLAGS+=	-I${NETBSDSRCDIR}/lib/libc/include

.ifndef HOSTPROG
LDADD+=	-lutil
DPADD+=	${LIBUTIL}
.endif

COPTS.pwd_mkdb.c+=	-Wno-format-nonliteral

.include <bsd.prog.mk>
