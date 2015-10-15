# $NetBSD: Makefile.inc,v 1.1 2014/08/10 05:47:37 matt Exp $

SRCS+=		rtld_start.S mdreloc.c

# XXX Should not be in CPPFLAGS!
CPPFLAGS+=	-fpic

CPPFLAGS+=	-DELFSIZE=64
CPPFLAGS+=	-DHAVE_INITFINI_ARRAY
CPPFLAGS+=	-DELF_NOTE_MARCH_DESC=\"${LDELFSO_MACHINE_ARCH}\"

LDFLAGS+=	-Wl,-e,_rtld_start
