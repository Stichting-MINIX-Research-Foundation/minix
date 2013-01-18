#	$NetBSD: Makefile,v 1.10 2010/02/03 15:34:46 roy Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	tput
DPADD=	${LIBTERMINFO}
LDADD=	-lterminfo
MLINKS=	tput.1 clear.1
SCRIPTS=clear.sh

.include <bsd.prog.mk>
