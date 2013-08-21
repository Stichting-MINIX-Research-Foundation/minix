#	$NetBSD: Makefile,v 1.6 2013/05/31 09:14:18 mrg Exp $

.include <bsd.own.mk>

PROG=	xwininfo
SRCS=	xwininfo.c clientwin.c dsimple.c

XWININFODIR=	${X11SRCDIR.${PROG}}
CPPFLAGS+=-I${XWININFODIR} -I${XWININFODIR}/../include -DICONV_CONST=const

.PATH:	${XWININFODIR} ${XWININFODIR}/man

LDADD+=	-lXmuu -lXt -lSM -lICE -lXext -lX11 -lxcb -lxcb-shape
DPADD+=	${LIBXMUU} ${LIBXT} ${LIBSM} ${LIBICE} ${LIBXEXT} ${LIBX11} ${LIBXCB} ${LIBXCB_SHAPE}

CWARNFLAGS.clang+=	-Wno-tautological-compare

.include <bsd.x11.mk>
.include <bsd.prog.mk>
