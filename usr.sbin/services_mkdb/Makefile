#	$NetBSD: Makefile,v 1.7 2010/04/25 00:54:46 joerg Exp $

PROG=	services_mkdb
MAN=	services_mkdb.8
SRCS=	services_mkdb.c output_cdb.c output_db.c uniq.c

LDADD+=	-lutil
DPADD+=	${LIBUTIL}
#COPTS+=-g

.include <bsd.prog.mk>
