#	$NetBSD: Makefile,v 1.1 2008/09/26 06:57:03 rtr Exp $

XMODULEDIR=	${X11USRLIBDIR}/modules/extensions

.include "../../../../Makefile.serverlib"
.include "../../../../Makefile.servermod"

LIB=	GLcore

.PATH:	${X11SRCDIR.xorg-server}/hw/xfree86/dixmods
SRCS=	${SRCS.GLcore} GLcoremodule.c

.include "../../../../GL/GLcore/Makefile.GLcore"
