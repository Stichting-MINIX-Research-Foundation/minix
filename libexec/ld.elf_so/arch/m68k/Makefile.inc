#	$NetBSD: Makefile.inc,v 1.10 2005/06/04 16:17:17 lukem Exp $

SRCS+=		rtld_start.S mdreloc.c

# XXX Should not be in CPPFLAGS!
CPPFLAGS+=	-fpic

CPPFLAGS+=	-DELFSIZE=32

LDFLAGS+=	-Wl,-e,.rtld_start
