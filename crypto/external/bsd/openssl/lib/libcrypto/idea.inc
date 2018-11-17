#	$NetBSD: idea.inc,v 1.4 2017/05/21 14:24:05 riastradh Exp $
#
#	@(#) Copyright (c) 1995 Simon J. Gerraty
#
#	SRCS extracted from src/crypto/dist/openssl/crypto/idea/Makefile
#

.PATH:	${OPENSSLSRC}/crypto/idea


IDEA_SRCS = i_cfb64.c i_ofb64.c i_ecb.c
SRCS+=${IDEA_SRCS} i_cbc.c i_skey.c

.for cryptosrc in ${IDEA_SRCS}
CPPFLAGS.${cryptosrc} = -I${OPENSSLSRC}/crypto/idea
.endfor
