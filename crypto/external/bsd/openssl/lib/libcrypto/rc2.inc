#	$NetBSD: rc2.inc,v 1.1 2009/07/19 23:30:44 christos Exp $
#
#	@(#) Copyright (c) 1995 Simon J. Gerraty
#
#	SRCS extracted from src/crypto/dist/openssl/crypto/rc2/Makefile
#

.PATH:	${OPENSSLSRC}/crypto/rc2


RC2_SRCS = rc2_ecb.c rc2_skey.c rc2_cbc.c rc2cfb64.c rc2ofb64.c
SRCS += ${RC2_SRCS}

.for cryptosrc in ${RC2_SRCS}
CPPFLAGS.${cryptosrc} = -I${OPENSSLSRC}/crypto/rc2
.endfor
