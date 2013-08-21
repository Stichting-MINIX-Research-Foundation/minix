#	$NetBSD: Makefile,v 1.2 2008/08/09 11:16:02 rtr Exp $

XMODULEDIR=	${X11USRLIBDIR}/modules

.include "../../../../Makefile.serverlib"
.include "../../../../Makefile.servermod"

LIB=	fb

.PATH:	${X11SRCDIR.xorg-server}/hw/xfree86/dixmods
SRCS=	${SRCS.fb} fbmodule.c fbcmap_mi.c

.include "../../../../fb/Makefile.fb"
