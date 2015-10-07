#	$NetBSD: stack.inc,v 1.1 2009/07/19 23:30:44 christos Exp $
#
#	@(#) Copyright (c) 1995 Simon J. Gerraty
#
#	SRCS extracted from src/crypto/dist/openssl/crypto/stack/Makefile
#

.PATH:	${OPENSSLSRC}/crypto/stack


STACK_SRCS = stack.c
SRCS += ${STACK_SRCS}

.for cryptosrc in ${STACK_SRCS}
CPPFLAGS.${cryptosrc} = -I${OPENSSLSRC}/crypto/stack
.endfor
