# $NetBSD: Makefile,v 1.4 2017/01/28 21:31:52 christos Exp $

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
ASN1_OPTS.hdb.asn1+= --one-code-file --sequence=HDB-Ext-KeySet --sequence=Keys

SRCS=			\
	common.c	\
	db.c		\
	db3.c		\
	dbinfo.c	\
	ext.c		\
	hdb-keytab.c	\
	hdb-ldap.c	\
	hdb-mitdb.c	\
	hdb-sqlite.c	\
	hdb.c		\
	hdb_err.c	\
	keys.c		\
	keytab.c	\
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
