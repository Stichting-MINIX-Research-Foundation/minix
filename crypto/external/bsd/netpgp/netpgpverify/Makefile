#	$NetBSD: Makefile,v 1.6 2012/07/10 19:38:17 christos Exp $

.include <bsd.own.mk>

PROG=		netpgpverify
BINDIR=		/usr/bin
SRCS=		verify.c

CPPFLAGS+=	-I${.CURDIR}/../dist/include -I${.CURDIR}/../lib

LIBNETPGPDIR!=	cd ${.CURDIR}/../lib && ${PRINTOBJDIR}
LDADD+=		-L${LIBNETPGPDIR} -lnetpgp
DPADD+=		${LIBNETPGPDIR}/libnetpgp.a

LIBMJDIR!=	cd ${.CURDIR}/../libmj && ${PRINTOBJDIR}
LDADD+=		-L${LIBMJDIR} -lmj
DPADD+=		${LIBMJDIR}/libmj.a

LDADD+=		-lcrypto -lz -lbz2
DPADD+=		${LIBCRYPTO} ${LIBZ} ${LIBBZ2}

MAN=		netpgpverify.1

EXTDIST=${.CURDIR}/../dist

.PATH: ${EXTDIST}/src/netpgpverify

.include <bsd.prog.mk>
