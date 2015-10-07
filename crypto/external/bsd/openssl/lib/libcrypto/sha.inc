#	$NetBSD: sha.inc,v 1.1 2009/07/19 23:30:44 christos Exp $
#
#	@(#) Copyright (c) 1995 Simon J. Gerraty
#
#	SRCS extracted from src/crypto/dist/openssl/crypto/sha/Makefile
#

.PATH:	${OPENSSLSRC}/crypto/sha


SHA_SRCS = sha_dgst.c sha1dgst.c sha_one.c sha1_one.c

# Replaced OpenSSL version to avoid overlap with libc
SHA_SRCS+= libc-sha512.c libc-sha256.c

SRCS += ${SHA_SRCS}

.for cryptosrc in ${SHA_SRCS}
CPPFLAGS.${cryptosrc} = -I${OPENSSLSRC}/crypto/sha
.endfor
