#	$NetBSD: dso.inc,v 1.2 2016/10/14 16:09:43 spz Exp $
#
#	@(#) Copyright (c) 1995 Simon J. Gerraty
#
#	SRCS extracted from src/crypto/dist/openssl/crypto/dso/Makefile
#

.PATH:	${OPENSSLSRC}/crypto/dso


DSO_SRCS = dso_dl.c dso_dlfcn.c dso_err.c dso_lib.c dso_null.c \
	dso_openssl.c dso_win32.c dso_vms.c dso_beos.c
SRCS += ${DSO_SRCS}

.for cryptosrc in ${DSO_SRCS}
CPPFLAGS.${cryptosrc} = -I${OPENSSLSRC}/crypto/dso
.endfor
