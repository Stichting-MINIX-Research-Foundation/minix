#	$NetBSD: md5.inc,v 1.2 2015/05/16 19:19:36 joerg Exp $
#
#	@(#) Copyright (c) 1995 Simon J. Gerraty
#
#	SRCS extracted from src/crypto/dist/openssl/crypto/md5/Makefile
#

.PATH:	${OPENSSLSRC}/crypto/md5


MD5_SRCS += md5_dgst.c md5_one.c $(MD5_ASM_OBJ)
SRCS += ${MD5_SRCS}

.for cryptosrc in ${MD5_SRCS}
CPPFLAGS.${cryptosrc} = -I${OPENSSLSRC}/crypto/md5 ${MD5CPPFLAGS}
.endfor
