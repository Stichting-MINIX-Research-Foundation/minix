#	$NetBSD: Makefile.inc,v 1.12 2012/08/15 03:46:07 matt Exp $

SRCS+=		rtld_start.S mdreloc.c

# XXX Should not be in CPPFLAGS!
CPPFLAGS+=	-fpic

CPPFLAGS+=	-DELFSIZE=32
.if ${MACHINE_ARCH} == "earm" || ${MACHINE_ARCH} == "earmeb"
CPPFLAGS+=	-DHAVE_INITFINI_ARRAY
.endif

LDFLAGS+=	-Wl,-e,_rtld_start
