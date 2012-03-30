#	$NetBSD: Makefile.inc,v 1.14 2009/12/13 09:31:47 mrg Exp $

SRCS+=		rtld_start.S mdreloc.c

# XXX Should not be in CPPFLAGS!
CPPFLAGS+=	-fpic

CPPFLAGS+=	-DELFSIZE=32

LDFLAGS+=	-Wl,-e,.rtld_start
