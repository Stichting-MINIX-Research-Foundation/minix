# $NetBSD: Makefile.inc,v 1.3 2011/10/16 17:24:32 joerg Exp $

XZHOMEPAGE=	http://tukaani.org/xz/
XZVERSION=	5.0.0

XZSRCDIR:=	${.PARSEDIR}/dist
XZINCDIR:=	${.PARSEDIR}/include

CPPFLAGS+=	-I${XZINCDIR}
CPPFLAGS+=	-I${XZSRCDIR}/src/common
CPPFLAGS+=	-I${XZSRCDIR}/src/liblzma/api
CPPFLAGS+=	-DHAVE_CONFIG_H

CFLAGS+=	-std=c99
WARNS=		2
