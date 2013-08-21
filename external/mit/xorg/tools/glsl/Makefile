#	$NetBSD: Makefile,v 1.1 2010/07/19 05:34:28 mrg Exp $

NOMAN=	1

.include <bsd.own.mk>

.PATH:	${X11SRCDIR.MesaLib}/src/glsl/apps

.include "Makefile.glslsrcs"

HOSTPROG=	glsl-compile

SRCS=	compile.c

SRCS+=	${SRCS.pp} ${SRCS.cl}

# also: process.c purify.c tokenise.c version.c

.include <bsd.hostprog.mk>
