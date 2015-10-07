#	$NetBSD: pkcs7.inc,v 1.1 2009/07/19 23:30:44 christos Exp $
#
#	@(#) Copyright (c) 1995 Simon J. Gerraty
#
#	SRCS extracted from src/crypto/dist/openssl/crypto/pkcs7/Makefile
#

.PATH:	${OPENSSLSRC}/crypto/pkcs7


PKCS7_SRCS = pk7_asn1.c pk7_lib.c pkcs7err.c pk7_doit.c pk7_smime.c pk7_attr.c \
	pk7_mime.c
SRCS += ${PKCS7_SRCS}

.for cryptosrc in ${PKCS7_SRCS}
CPPFLAGS.${cryptosrc} = -I${OPENSSLSRC}/crypto/pkcs7
.endfor
