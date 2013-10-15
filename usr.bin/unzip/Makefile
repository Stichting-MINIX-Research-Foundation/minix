# $NetBSD: Makefile,v 1.2 2011/08/18 11:29:27 christos Exp $

PROG=	unzip

DPADD+=		${LIBARCHIVE} ${LIBZ} ${LIBBZ2}
LDADD+=		-larchive -lz -lbz2

COPTS.unzip.c += -Wno-format-y2k

.include <bsd.prog.mk>
