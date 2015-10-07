# $NetBSD: Makefile,v 1.2 2011/05/25 19:21:18 he Exp $

.include <bsd.own.mk>
.include <${.CURDIR}/../../Makefile.inc>

.PATH:		${HEIMDIST}/kadmin

PROG=	kadmind
MAN=	kadmind.8

SRCS+= 	kadmind.c	\
	rpc.c		\
	server.c	\
	kadm_conn.c

CPPFLAGS+= 				\
	-I${HEIMDIST}/lib/krb5		\
	-I${HEIMDIST}/lib/asn1		\
	-I${HEIMBASE}/include/krb5

LDADD+= -lgssapi -lkadm5srv -lhdb -lutil
DPADD+= ${LIBGSSAPI} ${LIBKADM5SRV} ${LIBHDB} ${LIBUTIL}

.include <${HEIMBASE}/Makefile.rules.inc>
.include <bsd.prog.mk>
