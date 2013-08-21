#	$NetBSD: Makefile,v 1.2 2009/06/12 01:59:05 mrg Exp $

XMODULEDIR=	${X11USRLIBDIR}/modules/extensions

.include "../../../../Makefile.serverlib"
.include "../../../../Makefile.servermod"

LIB=	glx

.PATH:	${X11SRCDIR.xorg-server}/hw/xfree86/dixmods
SRCS=	${SRCS.glx} glxmodule.c

CPPFLAGS+=	-I${X11SRCDIR.xorg-server}/${LIB}

.include "../../../../glx/Makefile.glx"
