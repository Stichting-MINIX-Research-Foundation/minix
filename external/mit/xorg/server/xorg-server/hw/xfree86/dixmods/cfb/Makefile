#	$NetBSD: Makefile,v 1.2 2008/08/24 13:23:33 rtr Exp $

XMODULEDIR=     ${X11USRLIBDIR}/modules/extensions
PIXELSIZE=	8

.include "../../../../Makefile.serverlib"
.include "../../../../Makefile.servermod"

LIB=	cfb

.PATH:	${X11SRCDIR.xorg-server}/hw/xfree86/dixmods
SRCS=	${SRCS.cfb} cfbmodule.c

.include "../../../../cfb/Makefile.cfb"
