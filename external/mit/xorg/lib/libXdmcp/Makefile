#	$NetBSD: Makefile,v 1.4 2015/07/19 05:53:44 mrg Exp $

.include <bsd.own.mk>

LIB=	Xdmcp
.PATH:	${X11SRCDIR.${LIB}}
.PATH:	${X11SRCDIR.${LIB}}/include/X11

SRCS=	Array.c \
	Fill.c \
	Flush.c \
	Key.c \
	Read.c \
	Unwrap.c \
	Wrap.c \
	Write.c \
	Wraphelp.c

INCS=	Xdmcp.h
INCSDIR=${X11INCDIR}/X11

CPPFLAGS+=	${X11FLAGS.THREADLIB}
CPPFLAGS+=	-DHASXDMAUTH
CPPFLAGS+=	-DHAVE_ARC4RANDOM_BUF

PKGDIST=	${LIB}

.include <bsd.x11.mk>
.include <bsd.lib.mk>
