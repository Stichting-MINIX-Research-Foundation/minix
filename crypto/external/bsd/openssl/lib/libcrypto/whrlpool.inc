#	$NetBSD: whrlpool.inc,v 1.2 2015/05/16 19:33:59 joerg Exp $
#
#	@(#) Copyright (c) 1995 Simon J. Gerraty
#
#	SRCS extracted from src/crypto/dist/openssl/crypto/whrlpool/Makefile
#

.PATH:	${OPENSSLSRC}/crypto/whrlpool

.if empty(WHRLPOOL_SRCS)
WHRLPOOL_SRCS += wp_block.c
.endif
WHRLPOOL_SRCS += wp_dgst.c # wp_test.c   

SRCS += ${WHRLPOOL_SRCS}

.for cryptosrc in ${WHRLPOOL_SRCS}
CPPFLAGS.${cryptosrc} = -I${OPENSSLSRC}/crypto/whrlpool ${WHRLPOOLCPPFLAGS}
.endfor
CPPFLAGS.m_wp.c += -I${OPENSSLSRC}/crypto/whrlpool
