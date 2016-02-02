# $NetBSD: Makefile,v 1.3 2013/09/11 23:04:09 joerg Exp $

USE_FORT?= yes	# network protocol library

NOLINT=		# defined

.include <bsd.own.mk>
.include <${.CURDIR}/../../Makefile.inc>

.PATH:		${HEIMDIST}/lib/hdb ${HEIMBASE}/include

LIB=		hdb

LIBDPLIBS+=	hx509		${HEIMBASE}/lib/libhx509	\
		crypto		${SSLBASE}/lib/libcrypto	\
		krb5		${HEIMBASE}/lib/libkrb5		\
		asn1		${HEIMBASE}/lib/libasn1		\
		com_err		${HEIMBASE}/lib/libcom_err	\
		roken		${HEIMBASE}/lib/libroken	\
		wind		${HEIMBASE}/lib/libwind		\
		heimbase	${HEIMBASE}/lib/libheimbase

HEIMSRCS= hdb_err.et hdb.asn1

SRCS=			\
	common.c	\
	db.c		\
	db3.c		\
	ext.c		\
	hdb-ldap.c	\
	hdb.c		\
	hdb-keytab.c	\
	hdb-mitdb.c	\
	keys.c		\
	keytab.c	\
	dbinfo.c	\
	mkey.c		\
	ndbm.c		\
	print.c

INCSDIR= /usr/include/krb5
INCS += hdb.h hdb-protos.h ${COMPILE_ET_INCS} ${ASN1_INCS}

CPPFLAGS+=				\
	-I${HEIMBASE}/include/krb5	\
	-I${HEIMDIST}/lib/asn1		\
	-I${HEIMDIST}/lib/krb5		\
	-DHDB_DB_DIR=\"/var/heimdal\"	\

.include <${HEIMBASE}/Makefile.rules.inc>
.include <bsd.lib.mk>
