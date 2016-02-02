#	$NetBSD: pqueue.inc,v 1.1 2009/07/19 23:30:44 christos Exp $
#
#	@(#) Copyright (c) 1995 Simon J. Gerraty
#
#	SRCS extracted from src/crypto/dist/openssl/crypto/evp/Makefile
#

.PATH:	${OPENSSLSRC}/crypto/pqueue


PQUEUE_SRCS = pqueue.c
SRCS += ${PQUEUE_SRCS}

.for cryptosrc in ${PQUEUE_SRCS}
CPPFLAGS.${cryptosrc} = -I${OPENSSLSRC}/crypto/pqueue
.endfor
