#	$NetBSD: Makefile,v 1.29 2013/11/12 17:46:20 mbalmer Exp $
#	@(#)Makefile	8.3 (Berkeley) 7/24/94

# Missing: dungeon warp
# Moved: chess
# Don't belong: xneko xroach

.include <bsd.own.mk>

SUBDIR=	adventure arithmetic \
	bcd \
	colorbars \
	factor fortune \
	monop morse number \
	pig ppt primes \
	random tetris \
	wargames

.if !defined(__MINIX)
.if ${MKCXX} != "no"
SUBDIR+=	dab 
.endif
.endif # !defined(__MINIX)

.include <bsd.subdir.mk>
