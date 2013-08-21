#	$NetBSD: Makefile,v 1.9 2011/08/11 23:15:35 joerg Exp $

.include <bsd.own.mk>
.include "Makefile.common"

PROG=	xkbcomp
SRCS=	xkbcomp.c xkbscan.c expr.c vmod.c indicators.c misc.c \
	keymap.c keycodes.c keytypes.c compat.c action.c alias.c \
	symbols.c geometry.c xkbpath.c listing.c \
	xkbparse.y parseutils.c utils.c

CPPFLAGS.xkbpath.c=	-DDFLT_XKB_CONFIG_ROOT=\"${XKBROOTDIR}\"

CPPFLAGS+=		-I${X11SRCDIR.${PROG}} -I${X11SRCDIR.${PROG}}/../include -DHAVE_CONFIG_H
CPPFLAGS.geometry.c=	-Wno-error	# XXX
COPTS.xkbscan.c+=	-Wno-stack-protector

FILESDIR=${XKBROOTDIR}
FILES=	README

SYMLINKS+=	${BINDIR}/xkbcomp ${XKBROOTDIR}/xkbcomp
SYMLINKS+=	${XKBDATADIR} ${XKBCOMPDIR}

LDADD+=	-lxkbfile -lXext -lX11
DPADD+=	${LIBXKBFILE} ${LIBXEXT} ${LIBX11}

.PATH:	${X11SRCDIR.${PROG}}
.PATH:	${X11SRCDIR.${PROG}}/man

CWARNFLAGS.clang+=	-Wno-parentheses

.include <bsd.x11.mk>
.include <bsd.prog.mk>
