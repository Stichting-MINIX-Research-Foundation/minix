#	$NetBSD: Makefile,v 1.7 2007/05/28 12:06:24 tls Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

USE_FORT?=	yes	# cryptographic software

WARNS=	3
PROG=	bdes

LDADD+=	-lcrypt
DPADD+=	${LIBCRYPT}

.include <bsd.prog.mk>
