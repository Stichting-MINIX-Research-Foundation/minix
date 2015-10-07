#	$NetBSD: ocsp.inc,v 1.1 2009/07/19 23:30:44 christos Exp $
#
#	@(#) Copyright (c) 1995 Simon J. Gerraty
#
#	SRCS extracted from src/crypto/dist/openssl/crypto/ocsp/Makefile
#

.PATH:	${OPENSSLSRC}/crypto/ocsp


OCSP_SRCS = ocsp_asn.c ocsp_ext.c ocsp_ht.c ocsp_lib.c ocsp_cl.c \
	ocsp_srv.c ocsp_prn.c ocsp_vfy.c ocsp_err.c
SRCS += ${OCSP_SRCS}

.for cryptosrc in ${OCSP_SRCS}
CPPFLAGS.${cryptosrc} = -I${OPENSSLSRC}/crypto/ocsp
.endfor
