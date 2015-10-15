#	$NetBSD: Makefile.inc,v 1.1 2014/09/03 19:34:26 matt Exp $

SRCS+=		mdreloc.c
LDFLAGS+=	-Wl,-e,_rtld_start

CPPFLAGS+=	-fPIC

SRCS+=		rtld_start.S
CPPFLAGS+=	-DELFSIZE=32
#LDFLAGS+=	-Wl,--script,${.CURDIR}/arch/powerpc/ld.so.script
