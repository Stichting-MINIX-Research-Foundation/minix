#	$NetBSD: Makefile.inc,v 1.11 2009/09/19 14:54:17 skrll Exp $

SRCS+=		rtld_start.S mdreloc.c

# XXX Should not be in CPPFLAGS!
CPPFLAGS+=	-fpic

CPPFLAGS+=	-DELFSIZE=32

LDFLAGS+=	-Wl,-Bsymbolic -Wl,-e,_rtld_start
