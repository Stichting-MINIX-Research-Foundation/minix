#	$NetBSD: Makefile.inc,v 1.2 2011/09/16 16:41:20 joerg Exp $

WARNS=4

.include <bsd.own.mk>

BINDIR?= /usr/bin

IDIST=	${NETBSDSRCDIR}/external/bsd/byacc/dist

CPPFLAGS+= -DHAVE_CONFIG_H -I${.CURDIR}/../include -I${IDIST}
CWARNFLAGS+=	-Wno-missing-noreturn

.PATH: ${IDIST}
