# $NetBSD: Makefile,v 1.2 2011/05/25 19:21:18 he Exp $

.include <bsd.own.mk>
.include <${.CURDIR}/../../Makefile.inc>

.PATH:		${HEIMDIST}/kdc

PROG=	digest-service
SRCS=	digest-service.c
MAN=

CPPFLAGS+= 				\
	-I${HEIMBASE}/include/krb5	\
	-I${HEIMDIST}/lib/krb5		\
	-I${HEIMDIST}/lib/ipc

LDADD+= -lkdc -lhdb
DPADD+= ${LIBKDC} ${LIBHDB}

.include <${HEIMBASE}/Makefile.rules.inc>
.include <bsd.prog.mk>
