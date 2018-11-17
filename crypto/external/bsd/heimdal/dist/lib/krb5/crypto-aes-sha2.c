/*	$NetBSD: crypto-aes-sha2.c,v 1.2 2017/01/28 21:31:49 christos Exp $	*/

/*
 * Copyright (c) 1997 - 2008 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "krb5_locl.h"

/*
 * AES HMAC-SHA2
 */

krb5_error_code
_krb5_aes_sha2_md_for_enctype(krb5_context context,
			      krb5_enctype enctype,
			      const EVP_MD **md)
{
    switch (enctype) {
    case ETYPE_AES128_CTS_HMAC_SHA256_128:
	*md = EVP_sha256();
	break;
    case ETYPE_AES256_CTS_HMAC_SHA384_192:
	*md = EVP_sha384();
	break;
    default:
	return KRB5_PROG_ETYPE_NOSUPP;
	break;
    }
    return 0;
}

static krb5_error_code
SP_HMAC_SHA2_checksum(krb5_context context,
		      struct _krb5_key_data *key,
		      const void *data,
		      size_t len,
		      unsigned usage,
		      Checksum *result)
{
    krb5_error_code ret;
    const EVP_MD *md;
    unsigned char hmac[EVP_MAX_MD_SIZE];
    unsigned int hmaclen = sizeof(hmac);

    ret = _krb5_aes_sha2_md_for_enctype(context, key->key->keytype, &md);
    if (ret)
	return ret;

    HMAC(md, key->key->keyvalue.data, key->key->keyvalue.length,
	 data, len, hmac, &hmaclen);

    heim_assert(result->checksum.length <= hmaclen, "SHA2 internal error");

    memcpy(result->checksum.data, hmac, result->checksum.length);

    return 0;
}

static struct _krb5_key_type keytype_aes128_sha2 = {
    KRB5_ENCTYPE_AES128_CTS_HMAC_SHA256_128,
    "aes-128-sha2",
    128,
    16,
    sizeof(struct _krb5_evp_schedule),
    NULL,
    _krb5_evp_schedule,
    _krb5_AES_SHA2_salt,
    NULL,
    _krb5_evp_cleanup,
    EVP_aes_128_cbc
};

static struct _krb5_key_type keytype_aes256_sha2 = {
    KRB5_ENCTYPE_AES256_CTS_HMAC_SHA384_192,
    "aes-256-sha2",
    256,
    32,
    sizeof(struct _krb5_evp_schedule),
    NULL,
    _krb5_evp_schedule,
    _krb5_AES_SHA2_salt,
    NULL,
    _krb5_evp_cleanup,
    EVP_aes_256_cbc
};

struct _krb5_checksum_type _krb5_checksum_hmac_sha256_128_aes128 = {
    CKSUMTYPE_HMAC_SHA256_128_AES128,
    "hmac-sha256-128-aes128",
    64,
    16,
    F_KEYED | F_CPROOF | F_DERIVED,
    SP_HMAC_SHA2_checksum,
    NULL
};

struct _krb5_checksum_type _krb5_checksum_hmac_sha384_192_aes256 = {
    CKSUMTYPE_HMAC_SHA384_192_AES256,
    "hmac-sha384-192-aes256",
    128,
    24,
    F_KEYED | F_CPROOF | F_DERIVED,
    SP_HMAC_SHA2_checksum,
    NULL
};

static krb5_error_code
AES_SHA2_PRF(krb5_context context,
	     krb5_crypto crypto,
	     const krb5_data *in,
	     krb5_data *out)
{
    krb5_error_code ret;
    krb5_data label;
    const EVP_MD *md = NULL;

    ret = _krb5_aes_sha2_md_for_enctype(context, crypto->et->type, &md);
    if (ret)
	return ret;

    label.data = "prf";
    label.length = 3;

    ret = krb5_data_alloc(out, EVP_MD_size(md));
    if (ret)
	return ret;

    ret = _krb5_SP800_108_HMAC_KDF(context, &crypto->key.key->keyvalue,
				   &label, in, md, out);

    if (ret)
	krb5_data_free(out);

    return ret;
}

struct _krb5_encryption_type _krb5_enctype_aes128_cts_hmac_sha256_128 = {
    ETYPE_AES128_CTS_HMAC_SHA256_128,
    "aes128-cts-hmac-sha256-128",
    "aes128-cts-sha256",
    16,
    1,
    16,
    &keytype_aes128_sha2,
    NULL, /* should never be called */
    &_krb5_checksum_hmac_sha256_128_aes128,
    F_DERIVED | F_ENC_THEN_CKSUM | F_SP800_108_HMAC_KDF,
    _krb5_evp_encrypt_cts,
    16,
    AES_SHA2_PRF
};

struct _krb5_encryption_type _krb5_enctype_aes256_cts_hmac_sha384_192 = {
    ETYPE_AES256_CTS_HMAC_SHA384_192,
    "aes256-cts-hmac-sha384-192",
    "aes256-cts-sha384",
    16,
    1,
    16,
    &keytype_aes256_sha2,
    NULL, /* should never be called */
    &_krb5_checksum_hmac_sha384_192_aes256,
    F_DERIVED | F_ENC_THEN_CKSUM | F_SP800_108_HMAC_KDF,
    _krb5_evp_encrypt_cts,
    16,
    AES_SHA2_PRF
};
