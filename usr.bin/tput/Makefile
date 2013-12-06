#	$NetBSD: Makefile,v 1.11 2013/01/24 10:41:28 roy Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	tput
DPADD=	${LIBTERMINFO}
LDADD=	-lterminfo
MLINKS=	tput.1 clear.1
SCRIPTS=clear.sh

CPPFLAGS+=	-I${.CURDIR}/../../lib/libterminfo

.include <bsd.prog.mk>
