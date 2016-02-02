#	$NetBSD: txt_db.inc,v 1.1 2009/07/19 23:30:44 christos Exp $
#
#	@(#) Copyright (c) 1995 Simon J. Gerraty
#
#	SRCS extracted from src/crypto/dist/openssl/crypto/txt_db/Makefile
#

.PATH:	${OPENSSLSRC}/crypto/txt_db


TXT_DB_SRCS = txt_db.c
SRCS += ${TXT_DB_SRCS}

.for cryptosrc in ${TXT_DB_SRCS}
CPPFLAGS.${cryptosrc} = -I${OPENSSLSRC}/crypto/txt_db
.endfor
