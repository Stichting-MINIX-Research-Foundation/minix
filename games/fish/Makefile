#	$NetBSD: Makefile,v 1.10 1999/02/13 02:54:21 lukem Exp $
#	@(#)Makefile	8.1 (Berkeley) 5/31/93

.include <bsd.own.mk>

PROG=	fish
MAN=	fish.6
HIDEGAME=hidegame
.if ${MKSHARE} != "no"
FILES=fish.instr
FILESDIR=/usr/share/games
.endif

.include <bsd.prog.mk>
