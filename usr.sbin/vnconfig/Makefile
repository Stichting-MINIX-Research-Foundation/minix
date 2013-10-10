#	$NetBSD: Makefile,v 1.8 2013/06/09 22:00:50 jmmv Exp $

PROG=		vnconfig
MAN=		vnconfig.8
DPADD+=		${LIBUTIL}
LDADD+=		-lutil
LINKS+=		${BINDIR}/vnconfig ${BINDIR}/vndconfig
MLINKS+=	vnconfig.8 vndconfig.8

.include <bsd.prog.mk>
