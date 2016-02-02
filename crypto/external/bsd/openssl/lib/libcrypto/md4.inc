#	$NetBSD: md4.inc,v 1.1 2009/07/19 23:30:44 christos Exp $
#
#	@(#) Copyright (c) 1995 Simon J. Gerraty
#
#	SRCS extracted from src/crypto/dist/openssl/crypto/md4/Makefile
#

.PATH:	${OPENSSLSRC}/crypto/md4


MD4_SRCS = md4_dgst.c md4_one.c
SRCS += ${MD4_SRCS}

.for cryptosrc in ${MD4_SRCS}
CPPFLAGS.${cryptosrc} = -I${OPENSSLSRC}/crypto/md4
.endfor
