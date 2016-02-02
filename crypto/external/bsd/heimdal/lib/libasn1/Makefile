# $NetBSD: Makefile,v 1.2 2013/09/11 23:04:09 joerg Exp $

USE_FORT?= yes	# network protocol library

.include <bsd.own.mk>
.include <${.CURDIR}/../../Makefile.inc>

.PATH:		${HEIMDIST}/lib/asn1 ${HEIMBASE}/include

LIB=		asn1

LIBDPLIBS+=	com_err	${.CURDIR}/../libcom_err
LIBDPLIBS+=	roken	${.CURDIR}/../libroken

HEIMSRCS =		\
	asn1_err.et	\
	krb5.asn1	\
	cms.asn1	\
	rfc2459.asn1	\
	pkinit.asn1	\
	pkcs12.asn1	\
	pkcs8.asn1	\
	pkcs9.asn1	\
	digest.asn1	\
	kx509.asn1

ASN1_OPTS.rfc2459.asn1 = \
	--one-code-file				\
	--preserve-binary=TBSCertificate	\
	--preserve-binary=TBSCRLCertList	\
	--preserve-binary=Name			\
	--sequence=GeneralNames			\
	--sequence=Extensions			\
	--sequence=CRLDistributionPoints

INCSDIR= /usr/include/krb5

INCS=	asn1-common.h		\
	heim_asn1.h		\
	der.h			\
	der-protos.h		\
	${COMPILE_ET_INCS}	\
	${ASN1_INCS}

SRCS=			\
	der.c		\
	der_get.c	\
	der_put.c	\
	der_free.c	\
	der_length.c	\
	der_copy.c	\
	der_cmp.c	\
	der_format.c	\
	extra.c		\
	timegm.c

.if ${USETOOLS} != "yes"
# .PHONY: ${ASN1COMPILEOBJ}/asn1_compile
.PHONY: try_to_build_asn1_compile

try_to_build_asn1_compile:
	@cd ${.CURDIR}/asn1_compile && ${MAKE}

${ASN1COMPILEOBJ}/asn1_compile: try_to_build_asn1_compile

SUBDIR=	asn1_compile
.endif

.include <${HEIMBASE}/Makefile.rules.inc>
.include <bsd.lib.mk>
.if ${USETOOLS} != "yes"
.include <bsd.subdir.mk>
.endif
