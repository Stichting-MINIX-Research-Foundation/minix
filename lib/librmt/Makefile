#	$NetBSD: Makefile,v 1.9 2007/05/28 12:06:21 tls Exp $

.ifndef __MINIX
USE_FORT?= yes	# network protocol library
.endif

NOPIC=		# defined
NOPROFILE=	# defined

LIB=		rmt
SRCS=		rmtlib.c
MAN=		rmtops.3

.ifndef __MINIX
CPPFLAGS+=	-D_REENTRANT
.endif

.include <bsd.lib.mk>
