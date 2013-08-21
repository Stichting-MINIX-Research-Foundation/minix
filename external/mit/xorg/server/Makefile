#	$NetBSD: Makefile,v 1.8 2013/06/12 21:35:29 mrg Exp $

# Makefile for modular-xorg-server and drivers

.include <bsd.own.mk>

.include "xorg-server/Makefile.common"

SUBDIR=	xorg-server

# We only make drivers if we make the actual Xorg server,
# as opposed to Xvfb and Xnest.
.if ${MKXORG_SERVER} != "no"
SUBDIR+= drivers
.endif

.include <bsd.subdir.mk>
