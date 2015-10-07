#	$NetBSD: Makefile,v 1.2 2010/04/12 17:58:40 joerg Exp $

.include <bsd.own.mk>

USE_FORT?= yes	# cryptographic software

LIB=	des
SRCS=	ocbc_cksm.c ocbc_enc.c ocfb64ede.c ocfb64enc.c ocfb_enc.c \
	odes_enc.c oecb3_enc.c oecb_enc.c oede_cbcm_enc.c oenc_read.c \
	oenc_writ.c ofcrypt.c ofcrypt_b.c oofb64ede.c oofb64enc.c \
	oofb_enc.c opcbc_enc.c oqud_cksm.c orand_key.c oread2pwd.c \
	oread_pwd.c orpc_enc.c oset_key.c ostr2key.c oxcbc_enc.c

# NetBSD addition
SRCS+=	ornd_keys.c

CPPFLAGS+=-DOPENSSL_VERSION_PTEXT="\" based on OpenSSL 0.9.6j 10 Apr 2003\""
CPPFLAGS+=-DOPENSSL_cleanse=bzero -DOPENSSL_malloc=malloc

INCS=	des.h
INCSDIR=/usr/include

make-manpages:
	pod2man --section=3 --center=libdes --release=0.9.6j des.pod | \
	sed -e 's/[[:space:]]*$$//' > ${.CURDIR}/des.3
	pod2man --section=7 --center=libdes --release=0.9.6j des_modes.pod | \
	sed -e 's/[[:space:]]*$$//' > ${.CURDIR}/des_modes.7

MAN=	des.3 des_modes.7

MLINKS+=des.3 des_cbc_cksum.3 \
	des.3 des_cfb64_encrypt.3 \
	des.3 des_cfb_encrypt.3 \
	des.3 des_crypt.3 \
	des.3 des_ecb2_encrypt.3 \
	des.3 des_ecb3_encrypt.3 \
	des.3 des_ecb_encrypt.3 \
	des.3 des_ede2_cbc_encrypt.3 \
	des.3 des_ede2_cfb64_encrypt.3 \
	des.3 des_ede2_ofb64_encrypt.3 \
	des.3 des_ede3_cbc_encrypt.3 \
	des.3 des_ede3_cbcm_encrypt.3 \
	des.3 des_ede3_cfb64_encrypt.3 \
	des.3 des_ede3_ofb64_encrypt.3 \
	des.3 des_enc_read.3 \
	des.3 des_enc_write.3 \
	des.3 des_fcrypt.3 \
	des.3 des_is_weak_key.3 \
	des.3 des_key_sched.3 \
	des.3 des_ncbc_encrypt.3 \
	des.3 des_ofb64_encrypt.3 \
	des.3 des_ofb_encrypt.3 \
	des.3 des_pcbc_encrypt.3 \
	des.3 des_quad_cksum.3 \
	des.3 des_random_key.3 \
	des.3 des_read_2passwords.3 \
	des.3 des_read_password.3 \
	des.3 des_read_pw_string.3 \
	des.3 des_set_key.3 \
	des.3 des_set_key_checked.3 \
	des.3 des_set_key_unchecked.3 \
	des.3 des_set_odd_parity.3 \
	des.3 des_string_to_2keys.3 \
	des.3 des_string_to_key.3 \
	des.3 des_xcbc_encrypt.3

.include <bsd.lib.mk>
