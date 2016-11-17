#	$NetBSD: Makefile,v 1.5 2014/11/09 17:39:37 pooka Exp $
#

RUMPTOP=	${.CURDIR}/../../sys/rump

LIBDPLIBS+=     rumpuser ${.CURDIR}/../librumpuser

WARNS=		3	# XXX: kernel isn't ready for -Wsign-compare

.include "${RUMPTOP}/librump/rumpkern/Makefile.rumpkern"
