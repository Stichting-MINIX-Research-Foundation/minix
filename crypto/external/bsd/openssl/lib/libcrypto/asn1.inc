#	$NetBSD: asn1.inc,v 1.1 2009/07/19 23:30:42 christos Exp $
#
#	@(#) Copyright (c) 1995 Simon J. Gerraty
#
#	SRCS extracted from src/crypto/dist/openssl/crypto/asn1/Makefile
#

.PATH:	${OPENSSLSRC}/crypto/asn1


ASN1_SRCS = a_object.c a_bitstr.c a_utctm.c a_gentm.c a_time.c a_int.c a_octet.c \
	a_print.c a_type.c a_set.c a_dup.c a_d2i_fp.c a_i2d_fp.c \
	a_enum.c a_utf8.c a_sign.c a_digest.c a_verify.c a_mbstr.c a_strex.c \
	x_algor.c x_val.c x_pubkey.c x_sig.c x_req.c x_attrib.c x_bignum.c \
	x_long.c x_name.c x_x509.c x_x509a.c x_crl.c x_info.c x_spki.c nsseq.c \
	d2i_pu.c d2i_pr.c i2d_pu.c i2d_pr.c\
	t_req.c t_x509.c t_x509a.c t_crl.c t_pkey.c t_spki.c t_bitst.c \
	tasn_new.c tasn_fre.c tasn_enc.c tasn_dec.c tasn_utl.c tasn_typ.c \
	f_int.c f_string.c n_pkey.c \
	f_enum.c x_pkey.c a_bool.c x_exten.c \
	asn1_par.c asn1_lib.c asn1_err.c a_bytes.c a_strnid.c \
	evp_asn1.c asn_pack.c p5_pbe.c p5_pbev2.c p8_pkey.c asn_moid.c \
	asn1_gen.c asn_mime.c ameth_lib.c bio_ndef.c tasn_prn.c \
	bio_asn1.c x_nx509.c
SRCS += ${ASN1_SRCS}

.for cryptosrc in ${ASN1_SRCS}
CPPFLAGS.${cryptosrc} = -I${OPENSSLSRC}/crypto/asn1
.endfor
