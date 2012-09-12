#	$NetBSD: Makefile.inc,v 1.12 2011/10/26 15:56:58 chs Exp $

SRCS+=		rtld_start.S ppc_reloc.c

# XXX Should not be in CPPFLAGS!
CPPFLAGS+=	-fpic

CPPFLAGS+=	-DELFSIZE=32

LDFLAGS+=	-Wl,-e,_rtld_start
LDFLAGS+=	-Wl,--script,${.CURDIR}/arch/powerpc/ld.so.script
