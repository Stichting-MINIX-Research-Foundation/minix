#	$NetBSD: Makefile.inc,v 1.2 2009/10/26 11:17:16 christos Exp $

WARNS?=	1

.include <bsd.own.mk>

VERSION=2.5.35
BINDIR?= /usr/bin

IDIST=	${NETBSDSRCDIR}/external/bsd/flex/dist

CPPFLAGS+= -DHAVE_CONFIG_H -I${.CURDIR}/../include -I${IDIST} \
	-DLOCALEDIR=\"/usr/share/locale\"

.PATH: ${IDIST}
