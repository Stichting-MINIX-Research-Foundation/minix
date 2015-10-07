#	$NetBSD: ssl.inc,v 1.5 2013/02/06 17:03:51 christos Exp $
#
#	@(#) Copyright (c) 1995 Simon J. Gerraty
#
#	SRCS extracted from ../../dist/openssl/ssl/Makefile.ssl
#

.PATH:	${OPENSSLSRC}/ssl

CPPFLAGS+=	-I${OPENSSLSRC}/ssl

.if !defined(USELESS_CRYPTO)
SRCS+=	s2_meth.c  s2_srvr.c  s2_clnt.c  s2_lib.c  s2_enc.c s2_pkt.c
.else
CPPFLAGS+=	-DNO_SSL2
.endif

SRCS+=	bio_ssl.c
SRCS+=	d1_both.c d1_clnt.c d1_enc.c d1_lib.c d1_pkt.c d1_srtp.c d1_srvr.c
SRCS+=	kssl.c
SRCS+=	s23_clnt.c s23_lib.c s23_meth.c s23_pkt.c s23_srvr.c
SRCS+=	s3_both.c s3_cbc.c s3_clnt.c s3_enc.c s3_lib.c s3_meth.c s3_pkt.c \
	s3_srvr.c
SRCS+=	ssl_algs.c ssl_asn1.c ssl_cert.c ssl_ciph.c ssl_err.c ssl_err2.c \
	ssl_lib.c ssl_rsa.c ssl_sess.c ssl_stat.c ssl_txt.c
SRCS+=	t1_clnt.c t1_enc.c t1_lib.c t1_meth.c t1_reneg.c t1_srvr.c
SRCS+=	tls_srp.c
