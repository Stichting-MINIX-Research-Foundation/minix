#	$NetBSD: Makefile,v 1.2 2014/03/13 02:01:04 pooka Exp $
#

.PATH:	${.CURDIR}/../../../../net

LIB=	rumpdev_bpf

SRCS=	bpf.c bpf_filter.c
SRCS+=	bpf_component.c

CPPFLAGS+=	-I${.CURDIR}/../../../librump/rumpvfs

.include <bsd.lib.mk>
.include <bsd.klinks.mk>
