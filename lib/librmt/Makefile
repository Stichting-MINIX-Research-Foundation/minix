#	$NetBSD: Makefile,v 1.9 2007/05/28 12:06:21 tls Exp $

USE_FORT?= yes	# network protocol library

NOPIC=		# defined
NOPROFILE=	# defined

LIB=		rmt
SRCS=		rmtlib.c
MAN=		rmtops.3

.if !defined(__MINIX)
CPPFLAGS+=	-D_REENTRANT
.endif # !defined(__MINIX)

.include <bsd.lib.mk>
