# $NetBSD: Makefile,v 1.3 2012/06/09 09:22:21 martin Exp $

WARNS?=	4
PROG=  colorbars
MAN=   colorbars.6
DPADD=	${LIBCURSES} ${LIBTERMINFO}
LDADD=	-lcurses -lterminfo

.include <bsd.prog.mk>
