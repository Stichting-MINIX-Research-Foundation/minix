#	$NetBSD: krb5.inc,v 1.1 2009/07/19 23:30:43 christos Exp $
#
#	@(#) Copyright (c) 1995 Simon J. Gerraty
#
#	SRCS extracted from src/crypto/dist/openssl/crypto/krb5/Makefile
#

.PATH:	${OPENSSLSRC}/crypto/krb5


KRB5_SRCS = krb5_asn.c
SRCS += ${KRB5_SRCS}

.for cryptosrc in ${KRB5_SRCS}
CPPFLAGS.${cryptosrc} = -I${OPENSSLSRC}/crypto/krb5
.endfor
