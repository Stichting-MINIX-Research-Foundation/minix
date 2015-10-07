#	$NetBSD: des.inc,v 1.1 2009/07/19 23:30:43 christos Exp $
#
#	@(#) Copyright (c) 1995 Simon J. Gerraty
#
#	SRCS extracted from src/crypto/dist/openssl/crypto/des/Makefile
#

.PATH:	${OPENSSLSRC}/crypto/des

.if !defined(DES_SRCS)
DES_SRCS = des_enc.c fcrypt_b.c
.endif
DES_SRCS += cbc_cksm.c cbc_enc.c  cfb64enc.c cfb_enc.c  \
	ecb3_enc.c ecb_enc.c  enc_read.c enc_writ.c \
	fcrypt.c ofb64enc.c ofb_enc.c  pcbc_enc.c \
	qud_cksm.c rand_key.c rpc_enc.c  set_key.c  \
	xcbc_enc.c \
	str2key.c  cfb64ede.c ofb64ede.c ede_cbcm_enc.c \
	read2pwd.c
#	des_old.c des_old2.c 
SRCS += ${DES_SRCS}

.for cryptosrc in ${DES_SRCS}
CPPFLAGS.${cryptosrc} = -I${OPENSSLSRC}/crypto/des
.endfor
