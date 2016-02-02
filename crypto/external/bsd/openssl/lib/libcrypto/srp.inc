#	$NetBSD: srp.inc,v 1.1 2011/06/05 23:09:50 spz Exp $
#
#	SRCS extracted from src/crypto/dist/openssl/crypto/srp/Makefile
#

.PATH:	${OPENSSLSRC}/crypto/srp


SRP_SRCS = srp_lib.c srp_vfy.c
SRCS += ${SRP_SRCS}

.for cryptosrc in ${SRP_SRCS}
CPPFLAGS.${cryptosrc} = -I${OPENSSLSRC}/crypto/srp
.endfor
