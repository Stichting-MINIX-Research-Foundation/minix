#	$NetBSD: bf.inc,v 1.1 2009/07/19 23:30:42 christos Exp $
#
#	@(#) Copyright (c) 1995 Simon J. Gerraty
#
#	SRCS extracted from src/crypto/dist/openssl/crypto/bf/Makefile
#

.PATH:	${OPENSSLSRC}/crypto/bf

.if !defined(BF_SRCS)
BF_SRCS = bf_enc.c 
.endif
BF_SRCS += bf_skey.c bf_ecb.c bf_cfb64.c bf_ofb64.c 
SRCS += ${BF_SRCS}

.for cryptosrc in ${BF_SRCS}
CPPFLAGS.${cryptosrc} = -I${OPENSSLSRC}/crypto/bf
.endfor
