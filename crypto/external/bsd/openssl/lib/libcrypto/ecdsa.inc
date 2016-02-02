#	$NetBSD: ecdsa.inc,v 1.1 2009/07/19 23:30:43 christos Exp $
#
#	@(#) Copyright (c) 1995 Simon J. Gerraty
#
#	SRCS extracted from src/crypto/dist/openssl/crypto/ec/Makefile
#

.PATH:	${OPENSSLSRC}/crypto/ecdsa


ECDSA_SRCS = ecs_asn1.c ecs_err.c ecs_lib.c ecs_locl.h ecs_ossl.c ecs_sign.c \
	ecs_vrf.c 
SRCS += ${ECDSA_SRCS}

.for cryptosrc in ${ECDSA_SRCS}
CPPFLAGS.${cryptosrc} = -I${OPENSSLSRC}/crypto/ecdsa
.endfor
