#	$NetBSD: Makefile,v 1.1 2010/09/10 05:30:28 agc Exp $

.include <bsd.own.mk>

PROG=		paaclient
#BINDIR=		/usr/bin

SRCS=		b64.c libpaa.c client.c
CPPFLAGS+=	-I${.CURDIR}/../../dist/include -I${.CURDIR}/../../dist/src/pgp2ssh

LDADD+=		-lmj -lnetpgp -lcrypto -lz -lbz2
MKMAN=no
WARNS=		0

.PATH: ${.CURDIR}/../../dist/src/libpaa ${.CURDIR}/../../dist/src/pgp2ssh

.include <bsd.prog.mk>

