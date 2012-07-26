#	$NetBSD: Makefile,v 1.1 2007/11/04 23:28:19 tls Exp $

LIB=isccc

.include <bsd.own.mk>

.include "${NETBSDSRCDIR}/usr.sbin/bind/Makefile.inc"

DIST=	${IDIST}/lib/isccc
.include "${DIST}/api"

.PATH.c:	${DIST}

SRCS=	alist.c base64.c cc.c ccmsg.c lib.c result.c sexpr.c symtab.c \
	version.c

.include <bsd.lib.mk>
