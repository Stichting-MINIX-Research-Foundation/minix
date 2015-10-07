# $NetBSD: Makefile,v 1.2 2011/05/25 19:21:19 he Exp $

.include <bsd.own.mk>
.include <${.CURDIR}/../../Makefile.inc>

USE_FORT?= yes	# cryptographic software

.PATH: ${HEIMDIST}/kdc

PROG= kstash
SRCS= kstash.c
MAN=  kstash.8

CPPFLAGS+= -I${HEIMDIST}/lib/krb5 -I${DESTDIR}/usr/include/krb5

#	-I${HEIMBASE}/include		\
#	-I${HEIMDIST}/base		\
#	-I${HEIMDIST}/include		\
#	-I${HEIMDIST}/lib/wind		\
#	-I${HEIMDIST}/kdc		\
#	-I${HEIMBASE}/lib/libwind	\
#	-DHAVE_CONFIG_H

LDADD+=	-lhdb
DPADD+=	${LIBHDB}

.include <${HEIMBASE}/Makefile.rules.inc>
.include <bsd.prog.mk>
