# $NetBSD: Makefile.inc,v 1.1 2005/09/24 20:51:14 elad Exp $

# hash functions
.PATH: ${.CURDIR}/hash/md2

SRCS+=	md2.c md2hl.c

MAN+=	md2.3

MLINKS+=md2.3 MD2Init.3 md2.3 MD2Update.3 md2.3 MD2Final.3
MLINKS+=md2.3 MD2End.3  md2.3 MD2File.3   md2.3 MD2Data.3
MLINKS+=md2.3 MD2Transform.3  md2.3 MD2FileChunk.3
