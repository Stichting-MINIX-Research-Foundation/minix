#	$NetBSD: Makefile,v 1.2 2009/02/12 13:20:17 lukem Exp $
#

RUMPTOP=	${.CURDIR}/../../sys/rump

LIBDPLIBS+=     rump	${.CURDIR}/../librump

WARNS=		3	# XXX: kernel isn't ready for -Wsign-compare

.include "${RUMPTOP}/librump/rumpnet/Makefile.rumpnet"
