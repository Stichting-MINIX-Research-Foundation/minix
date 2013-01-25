#	$NetBSD: Makefile,v 1.1 2010/02/03 15:16:33 roy Exp $

PROG=		infocmp
WARNS=		4

CPPFLAGS+=	-I${.CURDIR}/../../lib/libterminfo
LDADD+=		-lterminfo
DPADD+=		${LIBTERMINFO}

.include <bsd.prog.mk>
