/*	$NetBSD: crypto-ec.c,v 1.2 2017/01/28 21:31:48 christos Exp $	*/

/*
 * Copyright (c) 2016 Kungliga Tekniska Högskolan
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

#include <config.h>

#ifdef HAVE_HCRYPTO_W_OPENSSL
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/rsa.h>
#include <openssl/bn.h>
#include <openssl/objects.h>
#define HEIM_NO_CRYPTO_HDRS
#endif /* HAVE_HCRYPTO_W_OPENSSL */

#include "hx_locl.h"

extern const AlgorithmIdentifier _hx509_signature_sha512_data;
extern const AlgorithmIdentifier _hx509_signature_sha384_data;
extern const AlgorithmIdentifier _hx509_signature_sha256_data;
extern const AlgorithmIdentifier _hx509_signature_sha1_data;

void
_hx509_private_eckey_free(void *eckey)
{
#ifdef HAVE_HCRYPTO_W_OPENSSL
    EC_KEY_free(eckey);
#endif
}

#ifdef HAVE_HCRYPTO_W_OPENSSL
static int
heim_oid2ecnid(heim_oid *oid)
{
    /*
     * Now map to openssl OID fun
     */

    if (der_heim_oid_cmp(oid, ASN1_OID_ID_EC_GROUP_SECP256R1) == 0)
	return NID_X9_62_prime256v1;
#ifdef NID_secp521r1
    else if (der_heim_oid_cmp(oid, ASN1_OID_ID_EC_GROUP_SECP521R1) == 0)
        return NID_secp521r1;
#endif
#ifdef NID_secp384r1
    else if (der_heim_oid_cmp(oid, ASN1_OID_ID_EC_GROUP_SECP384R1) == 0)
        return NID_secp384r1;
#endif
#ifdef NID_secp160r1
    else if (der_heim_oid_cmp(oid, ASN1_OID_ID_EC_GROUP_SECP160R1) == 0)
	return NID_secp160r1;
#endif
#ifdef NID_secp160r2
    else if (der_heim_oid_cmp(oid, ASN1_OID_ID_EC_GROUP_SECP160R2) == 0)
	return NID_secp160r2;
#endif

    return NID_undef;
}

static int
parse_ECParameters(hx509_context context,
		   heim_octet_string *parameters, int *nid)
{
    ECParameters ecparam;
    size_t size;
    int ret;

    if (parameters == NULL) {
	ret = HX509_PARSING_KEY_FAILED;
	hx509_set_error_string(context, 0, ret,
			       "EC parameters missing");
	return ret;
    }

    ret = decode_ECParameters(parameters->data, parameters->length,
			      &ecparam, &size);
    if (ret) {
	hx509_set_error_string(context, 0, ret,
			       "Failed to decode EC parameters");
	return ret;
    }

    if (ecparam.element != choice_ECParameters_namedCurve) {
	free_ECParameters(&ecparam);
	hx509_set_error_string(context, 0, ret,
			       "EC parameters is not a named curve");
	return HX509_CRYPTO_SIG_INVALID_FORMAT;
    }

    *nid = heim_oid2ecnid(&ecparam.u.namedCurve);
    free_ECParameters(&ecparam);
    if (*nid == NID_undef) {
	hx509_set_error_string(context, 0, ret,
			       "Failed to find matcing NID for EC curve");
	return HX509_CRYPTO_SIG_INVALID_FORMAT;
    }
    return 0;
}


/*
 *
 */

static int
ecdsa_verify_signature(hx509_context context,
		       const struct signature_alg *sig_alg,
		       const Certificate *signer,
		       const AlgorithmIdentifier *alg,
		       const heim_octet_string *data,
		       const heim_octet_string *sig)
{
    const AlgorithmIdentifier *digest_alg;
    const SubjectPublicKeyInfo *spi;
    heim_octet_string digest;
    int ret;
    EC_KEY *key = NULL;
    int groupnid;
    EC_GROUP *group;
    const unsigned char *p;
    long len;

    digest_alg = sig_alg->digest_alg;

    ret = _hx509_create_signature(context,
				  NULL,
				  digest_alg,
				  data,
				  NULL,
				  &digest);
    if (ret)
	return ret;

    /* set up EC KEY */
    spi = &signer->tbsCertificate.subjectPublicKeyInfo;

    if (der_heim_oid_cmp(&spi->algorithm.algorithm, ASN1_OID_ID_ECPUBLICKEY) != 0)
	return HX509_CRYPTO_SIG_INVALID_FORMAT;

    /*
     * Find the group id
     */

    ret = parse_ECParameters(context, spi->algorithm.parameters, &groupnid);
    if (ret) {
	der_free_octet_string(&digest);
	return ret;
    }

    /*
     * Create group, key, parse key
     */

    key = EC_KEY_new();
    group = EC_GROUP_new_by_curve_name(groupnid);
    EC_KEY_set_group(key, group);
    EC_GROUP_free(group);

    p = spi->subjectPublicKey.data;
    len = spi->subjectPublicKey.length / 8;

    if (o2i_ECPublicKey(&key, &p, len) == NULL) {
	EC_KEY_free(key);
	return HX509_CRYPTO_SIG_INVALID_FORMAT;
    }

    ret = ECDSA_verify(-1, digest.data, digest.length,
		       sig->data, sig->length, key);
    der_free_octet_string(&digest);
    EC_KEY_free(key);
    if (ret != 1) {
	ret = HX509_CRYPTO_SIG_INVALID_FORMAT;
	return ret;
    }

    return 0;
}

static int
ecdsa_create_signature(hx509_context context,
		       const struct signature_alg *sig_alg,
		       const hx509_private_key signer,
		       const AlgorithmIdentifier *alg,
		       const heim_octet_string *data,
		       AlgorithmIdentifier *signatureAlgorithm,
		       heim_octet_string *sig)
{
    const AlgorithmIdentifier *digest_alg;
    heim_octet_string indata;
    const heim_oid *sig_oid;
    unsigned int siglen;
    int ret;

    if (signer->ops && der_heim_oid_cmp(signer->ops->key_oid, ASN1_OID_ID_ECPUBLICKEY) != 0)
	_hx509_abort("internal error passing private key to wrong ops");

    sig_oid = sig_alg->sig_oid;
    digest_alg = sig_alg->digest_alg;

    if (signatureAlgorithm) {
        ret = _hx509_set_digest_alg(signatureAlgorithm, sig_oid,
                                    "\x05\x00", 2);
	if (ret) {
	    hx509_clear_error_string(context);
	    return ret;
	}
    }

    ret = _hx509_create_signature(context,
				  NULL,
				  digest_alg,
				  data,
				  NULL,
				  &indata);
    if (ret)
	goto error;

    sig->length = ECDSA_size(signer->private_key.ecdsa);
    sig->data = malloc(sig->length);
    if (sig->data == NULL) {
	der_free_octet_string(&indata);
	ret = ENOMEM;
	hx509_set_error_string(context, 0, ret, "out of memory");
	goto error;
    }

    siglen = sig->length;

    ret = ECDSA_sign(-1, indata.data, indata.length,
		     sig->data, &siglen, signer->private_key.ecdsa);
    der_free_octet_string(&indata);
    if (ret != 1) {
	ret = HX509_CMS_FAILED_CREATE_SIGATURE;
	hx509_set_error_string(context, 0, ret,
			       "ECDSA sign failed: %d", ret);
	goto error;
    }
    if (siglen > sig->length)
	_hx509_abort("ECDSA signature prelen longer the output len");

    sig->length = siglen;

    return 0;
 error:
    if (signatureAlgorithm)
	free_AlgorithmIdentifier(signatureAlgorithm);
    return ret;
}

static int
ecdsa_available(const hx509_private_key signer,
		const AlgorithmIdentifier *sig_alg)
{
    const struct signature_alg *sig;
    const EC_GROUP *group;
    BN_CTX *bnctx = NULL;
    BIGNUM *order = NULL;
    int ret = 0;

    if (der_heim_oid_cmp(signer->ops->key_oid, &asn1_oid_id_ecPublicKey) != 0)
	_hx509_abort("internal error passing private key to wrong ops");

    sig = _hx509_find_sig_alg(&sig_alg->algorithm);

    if (sig == NULL || sig->digest_size == 0)
	return 0;

    group = EC_KEY_get0_group(signer->private_key.ecdsa);
    if (group == NULL)
	return 0;

    bnctx = BN_CTX_new();
    order = BN_new();
    if (order == NULL)
	goto err;

    if (EC_GROUP_get_order(group, order, bnctx) != 1)
	goto err;

#if 0
    /* If anything, require a digest at least as wide as the EC key size */
    if (BN_num_bytes(order) > sig->digest_size)
#endif
	ret = 1;
 err:
    if (bnctx)
	BN_CTX_free(bnctx);
    if (order)
	BN_clear_free(order);

    return ret;
}

static int
ecdsa_private_key2SPKI(hx509_context context,
		       hx509_private_key private_key,
		       SubjectPublicKeyInfo *spki)
{
    memset(spki, 0, sizeof(*spki));
    return ENOMEM;
}

static int
ecdsa_private_key_export(hx509_context context,
			 const hx509_private_key key,
			 hx509_key_format_t format,
			 heim_octet_string *data)
{
    return HX509_CRYPTO_KEY_FORMAT_UNSUPPORTED;
}

static int
ecdsa_private_key_import(hx509_context context,
			 const AlgorithmIdentifier *keyai,
			 const void *data,
			 size_t len,
			 hx509_key_format_t format,
			 hx509_private_key private_key)
{
    const unsigned char *p = data;
    EC_KEY **pkey = NULL;
    EC_KEY *key;

    if (keyai->parameters) {
	EC_GROUP *group;
	int groupnid;
	int ret;

	ret = parse_ECParameters(context, keyai->parameters, &groupnid);
	if (ret)
	    return ret;

	key = EC_KEY_new();
	if (key == NULL)
	    return ENOMEM;

	group = EC_GROUP_new_by_curve_name(groupnid);
	if (group == NULL) {
	    EC_KEY_free(key);
	    return ENOMEM;
	}
	EC_GROUP_set_asn1_flag(group, OPENSSL_EC_NAMED_CURVE);
	if (EC_KEY_set_group(key, group) == 0) {
	    EC_KEY_free(key);
	    EC_GROUP_free(group);
	    return ENOMEM;
	}
	EC_GROUP_free(group);
	pkey = &key;
    }

    switch (format) {
    case HX509_KEY_FORMAT_DER:

	private_key->private_key.ecdsa = d2i_ECPrivateKey(pkey, &p, len);
	if (private_key->private_key.ecdsa == NULL) {
	    hx509_set_error_string(context, 0, HX509_PARSING_KEY_FAILED,
				   "Failed to parse EC private key");
	    return HX509_PARSING_KEY_FAILED;
	}
	private_key->signature_alg = ASN1_OID_ID_ECDSA_WITH_SHA256;
	break;

    default:
	return HX509_CRYPTO_KEY_FORMAT_UNSUPPORTED;
    }

    return 0;
}

static int
ecdsa_generate_private_key(hx509_context context,
			   struct hx509_generate_private_context *ctx,
			   hx509_private_key private_key)
{
    return ENOMEM;
}

static BIGNUM *
ecdsa_get_internal(hx509_context context,
		   hx509_private_key key,
		   const char *type)
{
    return NULL;
}

static const unsigned ecPublicKey[] ={ 1, 2, 840, 10045, 2, 1 };
const AlgorithmIdentifier _hx509_signature_ecPublicKey = {
    { 6, rk_UNCONST(ecPublicKey) }, NULL
};

static const unsigned ecdsa_with_sha256_oid[] ={ 1, 2, 840, 10045, 4, 3, 2 };
const AlgorithmIdentifier _hx509_signature_ecdsa_with_sha256_data = {
    { 7, rk_UNCONST(ecdsa_with_sha256_oid) }, NULL
};

static const unsigned ecdsa_with_sha384_oid[] ={ 1, 2, 840, 10045, 4, 3, 3 };
const AlgorithmIdentifier _hx509_signature_ecdsa_with_sha384_data = {
    { 7, rk_UNCONST(ecdsa_with_sha384_oid) }, NULL
};

static const unsigned ecdsa_with_sha512_oid[] ={ 1, 2, 840, 10045, 4, 3, 4 };
const AlgorithmIdentifier _hx509_signature_ecdsa_with_sha512_data = {
    { 7, rk_UNCONST(ecdsa_with_sha512_oid) }, NULL
};

static const unsigned ecdsa_with_sha1_oid[] ={ 1, 2, 840, 10045, 4, 1 };
const AlgorithmIdentifier _hx509_signature_ecdsa_with_sha1_data = {
    { 6, rk_UNCONST(ecdsa_with_sha1_oid) }, NULL
};

hx509_private_key_ops ecdsa_private_key_ops = {
    "EC PRIVATE KEY",
    ASN1_OID_ID_ECPUBLICKEY,
    ecdsa_available,
    ecdsa_private_key2SPKI,
    ecdsa_private_key_export,
    ecdsa_private_key_import,
    ecdsa_generate_private_key,
    ecdsa_get_internal
};

const struct signature_alg ecdsa_with_sha512_alg = {
    "ecdsa-with-sha512",
    ASN1_OID_ID_ECDSA_WITH_SHA512,
    &_hx509_signature_ecdsa_with_sha512_data,
    ASN1_OID_ID_ECPUBLICKEY,
    &_hx509_signature_sha512_data,
    PROVIDE_CONF|REQUIRE_SIGNER|RA_RSA_USES_DIGEST_INFO|
        SIG_PUBLIC_SIG|SELF_SIGNED_OK,
    0,
    NULL,
    ecdsa_verify_signature,
    ecdsa_create_signature,
    64
};

const struct signature_alg ecdsa_with_sha384_alg = {
    "ecdsa-with-sha384",
    ASN1_OID_ID_ECDSA_WITH_SHA384,
    &_hx509_signature_ecdsa_with_sha384_data,
    ASN1_OID_ID_ECPUBLICKEY,
    &_hx509_signature_sha384_data,
    PROVIDE_CONF|REQUIRE_SIGNER|RA_RSA_USES_DIGEST_INFO|
        SIG_PUBLIC_SIG|SELF_SIGNED_OK,
    0,
    NULL,
    ecdsa_verify_signature,
    ecdsa_create_signature,
    48
};

const struct signature_alg ecdsa_with_sha256_alg = {
    "ecdsa-with-sha256",
    ASN1_OID_ID_ECDSA_WITH_SHA256,
    &_hx509_signature_ecdsa_with_sha256_data,
    ASN1_OID_ID_ECPUBLICKEY,
    &_hx509_signature_sha256_data,
    PROVIDE_CONF|REQUIRE_SIGNER|RA_RSA_USES_DIGEST_INFO|
        SIG_PUBLIC_SIG|SELF_SIGNED_OK,
    0,
    NULL,
    ecdsa_verify_signature,
    ecdsa_create_signature,
    32
};

const struct signature_alg ecdsa_with_sha1_alg = {
    "ecdsa-with-sha1",
    ASN1_OID_ID_ECDSA_WITH_SHA1,
    &_hx509_signature_ecdsa_with_sha1_data,
    ASN1_OID_ID_ECPUBLICKEY,
    &_hx509_signature_sha1_data,
    PROVIDE_CONF|REQUIRE_SIGNER|RA_RSA_USES_DIGEST_INFO|
        SIG_PUBLIC_SIG|SELF_SIGNED_OK,
    0,
    NULL,
    ecdsa_verify_signature,
    ecdsa_create_signature,
    20
};

#endif /* HAVE_HCRYPTO_W_OPENSSL */

const AlgorithmIdentifier *
hx509_signature_ecPublicKey(void)
{
#ifdef HAVE_HCRYPTO_W_OPENSSL
    return &_hx509_signature_ecPublicKey;
#else
    return NULL;
#endif /* HAVE_HCRYPTO_W_OPENSSL */
}

const AlgorithmIdentifier *
hx509_signature_ecdsa_with_sha256(void)
{
#ifdef HAVE_HCRYPTO_W_OPENSSL
    return &_hx509_signature_ecdsa_with_sha256_data;
#else
    return NULL;
#endif /* HAVE_HCRYPTO_W_OPENSSL */
}
