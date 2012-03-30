#	$NetBSD: Makefile.inc,v 1.12 2005/06/04 16:17:17 lukem Exp $

SRCS+=		rtld_start.S alpha_reloc.c

# XXX Should not be in CPPFLAGS!
CPPFLAGS+=	-fpic -mno-fp-regs

CPPFLAGS+=	-DELFSIZE=64
#CPPFLAGS+=	-DRTLD_DEBUG_ALPHA

LDFLAGS+=	-Wl,-e,_rtld_start
