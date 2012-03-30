#	$NetBSD: Makefile.inc,v 1.11 2009/10/22 21:56:13 skrll Exp $

SRCS+=		rtld_start.S ppc_reloc.c

# XXX Should not be in CPPFLAGS!
CPPFLAGS+=	-fpic -msoft-float

CPPFLAGS+=	-DELFSIZE=32

LDFLAGS+=	-Wl,-e,_rtld_start
LDFLAGS+=	-Wl,--script,${.CURDIR}/arch/powerpc/ld.so.script
