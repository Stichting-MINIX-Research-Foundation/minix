#	$NetBSD: Makefile.inc,v 1.14 2009/12/13 09:31:47 mrg Exp $

SRCS+=		rtld_start.S mdreloc.c

# XXX Should not be in CPPFLAGS!
CPPFLAGS+=	-fpic

CPPFLAGS+=	-DELFSIZE=32

# Support DT_INIT_ARRAY/DT_FINI_ARRAY walks in addition to the legacy
# .init/.ctors flow (needed by binaries built with modern compilers).
CPPFLAGS+=	-DHAVE_INITFINI_ARRAY

LDFLAGS+=	-Wl,-e,.rtld_start
