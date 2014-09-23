#	$NetBSD: Makefile,v 1.10 2010/02/06 23:45:26 he Exp $
#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	snake
SRCS=	snake.c
MAN=	snake.6
DPADD=	${LIBM} ${LIBCURSES} ${LIBTERMINFO}
LDADD=	-lm -lcurses -lterminfo
HIDEGAME=hidegame
SETGIDGAME=yes

.include "../../Makefile.inc"
.include <bsd.prog.mk>
