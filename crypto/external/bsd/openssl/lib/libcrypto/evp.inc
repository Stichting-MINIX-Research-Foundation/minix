#	$NetBSD: evp.inc,v 1.5 2013/02/18 21:15:25 riastradh Exp $
#
#	@(#) Copyright (c) 1995 Simon J. Gerraty
#
#	SRCS extracted from src/crypto/dist/openssl/crypto/evp/Makefile
#

.PATH:	${OPENSSLSRC}/crypto/evp


EVP_SRCS = encode.c digest.c evp_enc.c evp_key.c evp_acnf.c \
	e_des.c e_bf.c e_idea.c e_des3.c \
	e_rc4.c e_aes.c names.c e_camellia.c \
	e_xcbc_d.c e_rc2.c e_cast.c e_rc5.c \
	m_null.c m_md2.c m_md4.c m_md5.c m_sha.c m_sha1.c \
	m_dss.c m_dss1.c m_mdc2.c m_ripemd.c \
	p_open.c p_seal.c p_sign.c p_verify.c p_lib.c p_enc.c p_dec.c \
	bio_md.c bio_b64.c bio_enc.c evp_cnf.c evp_err.c e_null.c \
	c_all.c c_allc.c c_alld.c evp_lib.c bio_ok.c \
	evp_pkey.c evp_pbe.c p5_crpt.c p5_crpt2.c m_ecdsa.c pmeth_lib.c \
	pmeth_fn.c m_sigver.c m_wp.c pmeth_gn.c evp_fips.c \
	e_aes_cbc_hmac_sha1.c e_rc4_hmac_md5.c
SRCS += ${EVP_SRCS}

.for cryptosrc in ${EVP_SRCS}
CPPFLAGS.${cryptosrc} = -I${OPENSSLSRC}/crypto/evp
.endfor

EVP_AES_SRCS = e_aes.c

# XXX Should do this too, but it requires some other asm changes.
#EVP_AES_SRCS += e_aes_cbc_hmac_sha1.c

.for cryptosrc in ${EVP_AES_SRCS}
CPPFLAGS.${cryptosrc} += ${AESCPPFLAGS}
.endfor

CPPFLAGS.e_aes.c += -I${OPENSSLSRC}/crypto/modes
