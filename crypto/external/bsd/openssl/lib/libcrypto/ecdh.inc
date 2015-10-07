#	$NetBSD: ecdh.inc,v 1.1 2009/07/19 23:30:43 christos Exp $
#
#	@(#) Copyright (c) 1995 Simon J. Gerraty
#
#	SRCS extracted from src/crypto/dist/openssl/crypto/ec/Makefile
#

.PATH:	${OPENSSLSRC}/crypto/ecdh


ECDH_SRCS = ech_err.c ech_key.c ech_lib.c ech_ossl.c 
SRCS += ${ECDH_SRCS}

.for cryptosrc in ${ECDH_SRCS}
CPPFLAGS.${cryptosrc} = -I${OPENSSLSRC}/crypto/ecdh
.endfor
