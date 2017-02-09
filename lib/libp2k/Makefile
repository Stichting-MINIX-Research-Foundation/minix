#	$NetBSD: Makefile,v 1.3 2013/09/11 23:04:10 joerg Exp $
#

LIB=		p2k
LIBDPLIBS+=	puffs	${.CURDIR}/../libpuffs		\
		rump	${.CURDIR}/../librump		\
		rumpvfs	${.CURDIR}/../librumpvfs	\
		ukfs	${.CURDIR}/../libukfs

INCS=		p2k.h
INCSDIR=	/usr/include/rump

SRCS=		p2k.c
MAN=		p2k.3

.include <bsd.lib.mk>
