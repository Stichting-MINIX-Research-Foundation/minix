#	$NetBSD: Makefile,v 1.1 2009/09/03 15:35:59 pooka Exp $
#

RUMPTOP=	${.CURDIR}/../../sys/rump

LIBDPLIBS+=	rump	${.CURDIR}/../librump

WARNS=		3	# XXX: kernel isn't ready for -Wsign-compare

.include "${RUMPTOP}/librump/rumpdev/Makefile.rumpdev"
