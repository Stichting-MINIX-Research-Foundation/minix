#	$NetBSD: x509.inc,v 1.1 2009/07/19 23:30:45 christos Exp $
#
#	@(#) Copyright (c) 1995 Simon J. Gerraty
#
#	SRCS extracted from src/crypto/dist/openssl/crypto/x509/Makefile
#

.PATH:	${OPENSSLSRC}/crypto/x509


X509_SRCS = x509_def.c x509_d2.c x509_r2x.c x509_cmp.c \
	x509_obj.c x509_req.c x509spki.c x509_vfy.c \
	x509_set.c x509cset.c x509rset.c x509_err.c \
	x509name.c x509_v3.c x509_ext.c x509_att.c \
	x509type.c x509_lu.c x_all.c x509_txt.c \
	x509_trs.c x509_vpm.c by_file.c by_dir.c 
SRCS += ${X509_SRCS}

.for cryptosrc in ${X509_SRCS}
CPPFLAGS.${cryptosrc} = -I${OPENSSLSRC}/crypto/x509
.endfor
