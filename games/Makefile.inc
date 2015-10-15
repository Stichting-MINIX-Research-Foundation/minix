#	$NetBSD: Makefile.inc,v 1.16 2014/03/23 00:17:40 dholland Exp $
#	@(#)Makefile.inc	8.1 (Berkeley) 5/31/93

MKHIDEGAME?=	no

.if defined(HIDEGAME) && (${MKHIDEGAME} != no) && defined(PROG)
BINDIR=		/usr/games/hide
BINGRP=		games
.if defined(SETGIDGAME)
USE_FORT?=	yes
BINMODE=	2550
.else
BINMODE=	550
.endif
SYMLINKS+=	dm /usr/games/${PROG}
.else
BINDIR=		/usr/games
.if defined(SETGIDGAME)
BINGRP=		games
BINMODE=	2555
.endif
.endif
# Note: do not bother with WARNS=6 until -Wconversion is either
# removed from WARNS=6 or rendered useful by improving gcc; as it is
# (with gcc48) it produces buckets of drivel.
WARNS?=		5
