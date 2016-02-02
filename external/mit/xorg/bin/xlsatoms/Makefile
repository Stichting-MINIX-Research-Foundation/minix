#	$NetBSD: Makefile,v 1.4 2015/07/19 23:15:05 mrg Exp $

.include <bsd.own.mk>

PROG=	xlsatoms

LDADD+=	-lxcb
DPADD+=	${LIBXCB}

.if defined(__MINIX)
LDADD+= -lmthread
DPADD+= ${LIBMTHREAD}
.endif # defined(__MINIX)

.PATH:	${X11SRCDIR.${PROG}}
.PATH:	${X11SRCDIR.${PROG}}/man

.include "../../xorg-pkg-ver.mk"

.include <bsd.x11.mk>
.include <bsd.prog.mk>
