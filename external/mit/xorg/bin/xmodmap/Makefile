#	$NetBSD: Makefile,v 1.5 2015/07/19 23:15:06 mrg Exp $

.include <bsd.own.mk>

PROG=	xmodmap
SRCS=	xmodmap.c pf.c handle.c exec.c

CPPFLAGS+=	-DHAVE_ASPRINTF -DHAVE_STRNCASECMP

LDADD+=	-lX11
DPADD+=	${LIBX11}

.PATH:	${X11SRCDIR.${PROG}} ${X11SRCDIR.${PROG}}/man

COPTS.handle.c+=-Wno-error	# uses XKeycodeToKeysym
COPTS.exec.c+=	-Wno-error	# uses XKeycodeToKeysym

.include "../../xorg-pkg-ver.mk"

.include <bsd.x11.mk>
.include <bsd.prog.mk>
