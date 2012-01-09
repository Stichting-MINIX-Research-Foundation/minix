#	$NetBSD: Makefile.inc,v 1.4 2009/08/14 19:23:54 dsl Exp $
#

SRCS+=	ffs.c

SRCS+=	divdi3.S divsi3.S modsi3.S moddi3.S udivdi3.S udivsi3.S
SRCS+=	umoddi3.S umodsi3.S bswap64.S bswap32.S bswap16.S ntohl.S htonl.S
SRCS+=	memcpy.S
