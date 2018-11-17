#	$NetBSD: mdc2.inc,v 1.4 2017/05/21 14:24:05 riastradh Exp $
#
#	@(#) Copyright (c) 1995 Simon J. Gerraty
#
#	SRCS extracted from src/crypto/dist/openssl/crypto/mdc2/Makefile
#

.PATH:	${OPENSSLSRC}/crypto/mdc2


MDC2_SRCS = mdc2_one.c
SRCS+=	${MDC2_SRCS} mdc2dgst.c

.for cryptosrc in ${MDC2_SRCS}
CPPFLAGS.${cryptosrc} = -I${OPENSSLSRC}/crypto/mdc2
.endfor
