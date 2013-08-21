#	$NetBSD: Makefile,v 1.2 2008/08/24 13:24:12 rtr Exp $

XMODULEDIR=     ${X11USRLIBDIR}/modules/extensions
PIXELSIZE=	32

.include "../../../../Makefile.serverlib"
.include "../../../../Makefile.servermod"

LIB=	cfb32

.PATH:	${X11SRCDIR.xorg-server}/hw/xfree86/dixmods
SRCS=	${SRCS.cfb} cfb32module.c

.include "../../../../cfb/Makefile.cfb"
