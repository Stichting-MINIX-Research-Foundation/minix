#	$NetBSD: Makefile,v 1.3 2012/03/22 23:46:27 joerg Exp $

WARNS?=		2

DRIVER=		xf86-input-ws
DRIVER_NAME=	ws_drv

SRCS=		ws.c emumb.c
MAN=		ws.4

CPPFLAGS+=	-DHAVE_CONFIG_H \
		-I${X11SRCDIR.${DRIVER}}/../include \
		-I${X11SRCDIR.${DRIVER}}/include

CWARNFLAGS.clang+=	-Wno-parentheses-equality

.include "../Makefile.xf86-driver"
