#	$NetBSD: Makefile,v 1.1 2010/02/03 15:16:32 roy Exp $

PROG=		tic
WARNS=		4

CPPFLAGS+=	-I${.CURDIR}/../../lib/libterminfo

.ifndef HOSTPROG
LDADD+=		-lterminfo
DPADD+=		${LIBTERMINFO}
.endif

.include <bsd.prog.mk>
