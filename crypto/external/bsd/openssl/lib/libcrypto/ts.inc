#	$NetBSD: ts.inc,v 1.1 2009/07/19 23:30:44 christos Exp $
#
#	@(#) Copyright (c) 1995 Simon J. Gerraty
#
#	SRCS extracted from src/crypto/dist/openssl/crypto/err/Makefile
#

.PATH:	${OPENSSLSRC}/crypto/ts


TS_SRCS = ts_err.c ts_req_utils.c ts_req_print.c ts_rsp_utils.c ts_rsp_print.c \
    ts_rsp_sign.c ts_rsp_verify.c ts_verify_ctx.c ts_lib.c ts_conf.c \
    ts_asn1.c

SRCS += ${TS_SRCS}

.for cryptosrc in ${TS_SRCS}
CPPFLAGS.${cryptosrc} = -I${OPENSSLSRC}/crypto/ts
.endfor
