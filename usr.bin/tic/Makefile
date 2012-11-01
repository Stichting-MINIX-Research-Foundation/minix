#	$NetBSD: Makefile,v 1.2 2012/05/31 20:10:06 joerg Exp $

PROG=		tic
WARNS=		4

CPPFLAGS+=	-I${.CURDIR}/../../lib/libterminfo

.ifndef HOSTPROG
LDADD+=		-lterminfo -lutil
DPADD+=		${LIBTERMINFO} ${LIBUTIL}
.endif

.include <bsd.prog.mk>
