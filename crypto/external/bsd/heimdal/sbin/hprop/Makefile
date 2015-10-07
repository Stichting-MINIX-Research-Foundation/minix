# $NetBSD: Makefile,v 1.2 2011/05/25 19:21:19 he Exp $

.include <bsd.own.mk>
.include <${.CURDIR}/../../Makefile.inc>

USE_FORT?= yes		# network client

.PATH: ${HEIMDIST}/kdc

PROG= hprop

SRCS+= 	hprop.c mit_dump.c

MAN=	hprop.8

CPPFLAGS+= 				\
	-I${HEIMBASE}/include/krb5	\
	-I${HEIMBASE}/lib/libkrb5	\
	-I${HEIMDIST}/lib/asn1		\
	-I${HEIMDIST}/lib/hdb		\
	-I${HEIMDIST}/lib/krb5

LDADD+= -lhdb -lutil
DPADD+= ${LIBHDB} ${LIBUTIL}

.include <${HEIMBASE}/Makefile.rules.inc>
.include <bsd.prog.mk>
