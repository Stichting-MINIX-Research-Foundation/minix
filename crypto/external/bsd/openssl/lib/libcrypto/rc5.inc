#	$NetBSD: rc5.inc,v 1.2 2017/05/21 14:20:44 riastradh Exp $
#
#	@(#) Copyright (c) 1995 Simon J. Gerraty
#
#	SRCS extracted from src/crypto/dist/openssl/crypto/rc5/Makefile
#

.PATH:	${OPENSSLSRC}/crypto/rc5


RC5_SRCS = rc5_ecb.c rc5cfb64.c rc5ofb64.c
RC5_SRCS += rc5_skey.c rc5_enc.c
SRCS += ${RC5_SRCS}

.for cryptosrc in ${RC5_SRCS}
CPPFLAGS.${cryptosrc} = -I${OPENSSLSRC}/crypto/rc5
.endfor
