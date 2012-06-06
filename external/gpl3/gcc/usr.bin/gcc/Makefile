#	$NetBSD: Makefile,v 1.2 2011/06/29 02:00:09 mrg Exp $

.include <bsd.own.mk>

PROG=           gcc
SRCS=		gccspec.c

CPPFLAGS+=	-I${BACKENDOBJ}

LINKS+=		${BINDIR}/gcc ${BINDIR}/cc 

MAN=		gcc.1
MLINKS=		gcc.1 cc.1 gcc.1 g++.1 gcc.1 c++.1

TEXINFO=	gcc.texi
INFOFLAGS=      -I${DIST}/gcc/doc -I${DIST}/gcc/doc/include

.include "../Makefile.frontend"

.PATH: ${DIST}/gcc ${DIST}/gcc/doc

.include <bsd.info.mk>

COPTS.builtins.c = -Wno-stack-protector

LDADD+=		-lintl
DPADD+=		${LIBINTL}

gcc.info: gcc-vers.texi
