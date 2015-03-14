#	$NetBSD: Makefile,v 1.18 2013/08/11 03:44:27 dholland Exp $
#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	rogue
CPPFLAGS+=-DUNIX
SRCS=	hit.c init.c inventory.c level.c machdep.c main.c \
	message.c monster.c move.c object.c pack.c play.c random.c ring.c \
	room.c save.c score.c spec_hit.c throw.c trap.c use.c zap.c
DPADD=	${LIBCURSES} ${LIBTERMINFO}
LDADD=	-lcurses -lterminfo
HIDEGAME=hidegame
SETGIDGAME=yes
MAN=	rogue.6

.if make(install)
SUBDIR+=USD.doc
.endif

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
