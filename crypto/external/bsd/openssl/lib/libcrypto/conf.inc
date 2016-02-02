#	$NetBSD: conf.inc,v 1.1 2009/07/19 23:30:43 christos Exp $
#
#	@(#) Copyright (c) 1995 Simon J. Gerraty
#
#	SRCS extracted from src/crypto/dist/openssl/crypto/conf/Makefile
#

.PATH:	${OPENSSLSRC}/crypto/conf


CONF_SRCS = conf_err.c conf_lib.c conf_api.c conf_def.c conf_mod.c \
	conf_mall.c conf_sap.c
SRCS += ${CONF_SRCS}

.for cryptosrc in ${CONF_SRCS}
CPPFLAGS.${cryptosrc} = -I${OPENSSLSRC}/crypto/conf
.endfor
