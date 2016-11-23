# $NetBSD: Makefile,v 1.4 2015/08/24 23:04:43 pooka Exp $
#

.PATH:	${.CURDIR}/../../../../net ${.CURDIR}/../../../../netmpls

LIB=	rumpnet_netmpls
IOCONF=	NETMPLS.ioconf
SRCS=	mpls_ttl.c if_mpls.c
SRCS+=	netmpls_component.c

.include <bsd.lib.mk>
.include <bsd.klinks.mk>
