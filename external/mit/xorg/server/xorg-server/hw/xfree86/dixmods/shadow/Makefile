#	$NetBSD: Makefile,v 1.2 2008/08/24 13:37:33 rtr Exp $

XMODULEDIR=     ${X11USRLIBDIR}/modules/extensions

.include "../../../../Makefile.serverlib"
.include "../../../../Makefile.servermod"

LIB=	shadow

.PATH:	${X11SRCDIR.xorg-server}/hw/xfree86/dixmods
SRCS=	${SRCS.shadow} shmodule.c

.include "../../../../miext/shadow/Makefile.shadow"
