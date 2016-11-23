#	$NetBSD: Makefile.inc,v 1.5 2015/08/24 22:31:34 pooka Exp $
#

RUMPTOP=	${.CURDIR}/../../..
CPPFLAGS+=	-I${RUMPTOP}/librump/rumpkern -I${RUMPTOP}/librump/rumpnet

.include "${RUMPTOP}/Makefile.rump"
