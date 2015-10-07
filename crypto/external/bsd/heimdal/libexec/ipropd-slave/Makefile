# $NetBSD: Makefile,v 1.2 2011/05/25 19:21:18 he Exp $

.include <bsd.own.mk>
.include <${.CURDIR}/../../Makefile.inc>

.PATH:		${HEIMDIST}/lib/kadm5

PROG=		ipropd-slave

# man page included with ipropd-master
MAN=

SRCS+= 	ipropd_slave.c ipropd_common.c

CPPFLAGS+= 				\
	-I${HEIMDIST}/lib/krb5		\
	-I${HEIMDIST}/lib/asn1		\
	-I${DESTDIR}/usr/include/kadm5

LDADD+= -lkadm5srv -lhdb -lutil
DPADD+= ${LIBKADM5SRV} ${LIBHDB} ${LIBUTIL}

.include <${HEIMBASE}/Makefile.rules.inc>
.include <bsd.prog.mk>
