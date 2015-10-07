#	$NetBSD: cast.inc,v 1.1 2009/07/19 23:30:43 christos Exp $
#
#	@(#) Copyright (c) 1995 Simon J. Gerraty
#
#	SRCS extracted from src/crypto/dist/openssl/crypto/cast/Makefile
#

.PATH:	${OPENSSLSRC}/crypto/cast

.if !defined(CAST_SRCS)
CAST_SRCS = c_enc.c
.endif

CAST_SRCS += c_skey.c c_ecb.c c_cfb64.c c_ofb64.c 
SRCS += ${CAST_SRCS}

.for cryptosrc in ${CAST_SRCS}
CPPFLAGS.${cryptosrc} = -I${OPENSSLSRC}/crypto/cast
.endfor
