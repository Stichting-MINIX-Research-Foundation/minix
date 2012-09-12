#	$NetBSD: Makefile,v 1.3 2011/09/21 18:15:59 jakllsch Exp $

PROG=		gptmbr.bin
SRCS=		gptmbr.S

AFLAGS.gptmbr.S= ${${ACTIVE_CC} == "clang":?-no-integrated-as:}

.include <../Makefile.mbr>

LOADADDR=	0x600
