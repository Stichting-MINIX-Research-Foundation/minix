#	$NetBSD: whrlpool.inc,v 1.1 2012/07/26 19:58:40 christos Exp $
#
#	@(#) Copyright (c) 1995 Simon J. Gerraty
#
#	SRCS extracted from src/crypto/dist/openssl/crypto/whrlpool/Makefile
#

.PATH:	${OPENSSLSRC}/crypto/whrlpool

WHRLPOOL_SRCS = wp_block.c wp_dgst.c # wp_test.c   

SRCS += ${WHRLPOOL_SRCS}

.for cryptosrc in ${WHRLPOOL_SRCS}
CPPFLAGS.${cryptosrc} = -I${OPENSSLSRC}/crypto/whrlpool
.endfor
CPPFLAGS.m_wp.c += -I${OPENSSLSRC}/crypto/whrlpool
