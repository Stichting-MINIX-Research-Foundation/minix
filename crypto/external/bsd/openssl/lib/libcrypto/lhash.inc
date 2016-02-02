#	$NetBSD: lhash.inc,v 1.1 2009/07/19 23:30:43 christos Exp $
#
#	@(#) Copyright (c) 1995 Simon J. Gerraty
#
#	SRCS extracted from src/crypto/dist/openssl/crypto/lhash/Makefile
#

.PATH:	${OPENSSLSRC}/crypto/lhash


LHASH_SRCS = lhash.c lh_stats.c
SRCS += ${LHASH_SRCS}

.for cryptosrc in ${LHASH_SRCS}
CPPFLAGS.${cryptosrc} = -I${OPENSSLSRC}/crypto/lhash
.endfor
