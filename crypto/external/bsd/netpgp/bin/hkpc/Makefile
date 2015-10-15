# $NetBSD: Makefile,v 1.3 2014/12/05 04:52:01 agc Exp $

.include <bsd.own.mk>

PROG=hkpc
BINDIR=/usr/bin
SRCS=hkpc.c main.c

LIBNETPGPDIR!=	cd ${.CURDIR}/../../lib/netpgp && ${PRINTOBJDIR}
LDADD+=		-L${LIBNETPGPDIR} -lnetpgp
DPADD+=		${LIBNETPGPDIR}/libnetpgp.a

LIBMJDIR!=	cd ${.CURDIR}/../../libmj && ${PRINTOBJDIR}
LDADD+=		-L${LIBMJDIR} -lmj
DPADD+=		${LIBMJDIR}/libmj.a

LDADD+=		-lcrypto -lz -lbz2
DPADD+=		${LIBCRYPTO} ${LIBZ} ${LIBBZ2}

WARNS=0
MAN=hkpc.1

.PATH: ${.CURDIR}/../../dist/src/hkpclient

.include <bsd.prog.mk>
