#	$NetBSD: Makefile,v 1.29 2013/11/12 17:46:20 mbalmer Exp $
#	@(#)Makefile	8.3 (Berkeley) 7/24/94

# Missing: dungeon warp
# Moved: chess
# Don't belong: xneko xroach

.include <bsd.own.mk>

#MINIX: Not everything yet
SUBDIR=	adventure arithmetic \
	banner bcd \
	colorbars caesar \
	factor fish fortune \
	monop morse number \
	pig ppt primes \
	rain random rogue snake tetris \
	wargames worm worms wtf

.if !defined(__MINIX)
.if ${MKCXX} != "no"
SUBDIR+=	dab 
.endif
.endif # !defined(__MINIX)

.include <bsd.subdir.mk>
