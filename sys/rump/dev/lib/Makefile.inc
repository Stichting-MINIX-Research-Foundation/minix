#	$NetBSD: Makefile.inc,v 1.4 2015/08/24 23:01:58 pooka Exp $
#

RUMPTOP=	${.CURDIR}/../../..
CPPFLAGS+=	-I${RUMPTOP}/librump/rumpkern -I${RUMPTOP}/librump/rumpdev

.include "${RUMPTOP}/Makefile.rump"
