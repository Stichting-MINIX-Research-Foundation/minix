#	$NetBSD: Makefile,v 1.1 2007/11/04 23:28:19 tls Exp $

LIB=isccfg

.include <bsd.own.mk>

.include "${NETBSDSRCDIR}/lib/libbindexport/Makefile.inc"

DIST=	${IDIST}/lib/isccfg
.include "${DIST}/api"

.PATH.c:	${DIST}/unix ${DIST}

SRCS=	aclconf.c log.c namedconf.c parser.c version.c 

.include <bsd.lib.mk>
