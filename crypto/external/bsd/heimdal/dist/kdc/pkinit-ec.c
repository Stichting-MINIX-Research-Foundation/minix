/*	$NetBSD: pkinit-ec.c,v 1.2 2017/01/28 21:31:44 christos Exp $	*/

/*
 * Copyright (c) 2016 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2009 Apple Inc. All rights reserved.
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
#include <krb5/roken.h>

#ifdef PKINIT

/*
 * As with the other *-ec.c files in Heimdal, this is a bit of a hack.
 *
 * The idea is to use OpenSSL for EC because hcrypto doesn't have the
 * required functionality at this time.  To do this we segregate
 * EC-using code into separate source files and then we arrange for them
 * to get the OpenSSL headers and not the conflicting hcrypto ones.
 *
 * Because of auto-generated *-private.h headers, we end up needing to
 * make sure various types are defined before we include them, thus the
 * strange header include order here.
 */

#ifdef HAVE_HCRYPTO_W_OPENSSL
#include <openssl/ec.h>
#include <openssl/ecdh.h>
#include <openssl/evp.h>
#include <openssl/bn.h>
#define HEIM_NO_CRYPTO_HDRS
#else
#include <hcrypto/des.h>
#endif /* HAVE_HCRYPTO_W_OPENSSL */

#define NO_HCRYPTO_POLLUTION

#include "kdc_locl.h"
#include <krb5/heim_asn1.h>
#include <krb5/rfc2459_asn1.h>
#include <krb5/cms_asn1.h>
#include <krb5/pkinit_asn1.h>

#include <krb5/hx509.h>

#ifdef HAVE_HCRYPTO_W_OPENSSL
static void
free_client_ec_param(krb5_context context,
                     EC_KEY *ec_key_pk,
                     EC_KEY *ec_key_key)
{
    if (ec_key_pk != NULL)
        EC_KEY_free(ec_key_pk);
    if (ec_key_key != NULL)
        EC_KEY_free(ec_key_key);
}
#endif

void
_kdc_pk_free_client_ec_param(krb5_context context,
                             void *ec_key_pk,
                             void *ec_key_key)
{
#ifdef HAVE_HCRYPTO_W_OPENSSL
    free_client_ec_param(context, ec_key_pk, ec_key_key);
#endif
}

#ifdef HAVE_HCRYPTO_W_OPENSSL
static krb5_error_code
generate_ecdh_keyblock(krb5_context context,
                       EC_KEY *ec_key_pk,    /* the client's public key */
                       EC_KEY **ec_key_key,  /* the KDC's ephemeral private */
                       unsigned char **dh_gen_key, /* shared secret */
                       size_t *dh_gen_keylen)
{
    const EC_GROUP *group;
    EC_KEY *ephemeral;
    krb5_keyblock key;
    krb5_error_code ret;
    unsigned char *p;
    size_t size;
    int len;

    *dh_gen_key = NULL;
    *dh_gen_keylen = 0;
    *ec_key_key = NULL;

    memset(&key, 0, sizeof(key));

    if (ec_key_pk == NULL) {
        ret = KRB5KRB_ERR_GENERIC;
        krb5_set_error_message(context, ret, "public_key");
        return ret;
    }

    group = EC_KEY_get0_group(ec_key_pk);
    if (group == NULL) {
        ret = KRB5KRB_ERR_GENERIC;
        krb5_set_error_message(context, ret, "failed to get the group of "
                               "the client's public key");
        return ret;
    }

    ephemeral = EC_KEY_new();
    if (ephemeral == NULL)
        return krb5_enomem(context);

    EC_KEY_set_group(ephemeral, group);

    if (EC_KEY_generate_key(ephemeral) != 1) {
	EC_KEY_free(ephemeral);
        return krb5_enomem(context);
    }

    size = (EC_GROUP_get_degree(group) + 7) / 8;
    p = malloc(size);
    if (p == NULL) {
        EC_KEY_free(ephemeral);
        return krb5_enomem(context);
    }

    len = ECDH_compute_key(p, size,
                           EC_KEY_get0_public_key(ec_key_pk),
                           ephemeral, NULL);
    if (len <= 0) {
        free(p);
        EC_KEY_free(ephemeral);
        ret = KRB5KRB_ERR_GENERIC;
        krb5_set_error_message(context, ret, "Failed to compute ECDH "
                               "public shared secret");
        return ret;
    }

    *ec_key_key = ephemeral;
    *dh_gen_key = p;
    *dh_gen_keylen = len;

    return 0;
}
#endif /* HAVE_HCRYPTO_W_OPENSSL */

krb5_error_code
_kdc_generate_ecdh_keyblock(krb5_context context,
                            void *ec_key_pk,    /* the client's public key */
                            void **ec_key_key,  /* the KDC's ephemeral private */
                            unsigned char **dh_gen_key, /* shared secret */
                            size_t *dh_gen_keylen)
{
#ifdef HAVE_HCRYPTO_W_OPENSSL
    return generate_ecdh_keyblock(context, ec_key_pk,
                                  (EC_KEY **)ec_key_key,
                                  dh_gen_key, dh_gen_keylen);
#else
    return ENOTSUP;
#endif /* HAVE_HCRYPTO_W_OPENSSL */
}

#ifdef HAVE_HCRYPTO_W_OPENSSL
static krb5_error_code
get_ecdh_param(krb5_context context,
               krb5_kdc_configuration *config,
               SubjectPublicKeyInfo *dh_key_info,
               EC_KEY **out)
{
    ECParameters ecp;
    EC_KEY *public = NULL;
    krb5_error_code ret;
    const unsigned char *p;
    size_t len;
    int nid;

    if (dh_key_info->algorithm.parameters == NULL) {
	krb5_set_error_message(context, KRB5_BADMSGTYPE,
			       "PKINIT missing algorithm parameter "
			       "in clientPublicValue");
	return KRB5_BADMSGTYPE;
    }

    memset(&ecp, 0, sizeof(ecp));

    ret = decode_ECParameters(dh_key_info->algorithm.parameters->data,
			      dh_key_info->algorithm.parameters->length, &ecp, &len);
    if (ret)
	goto out;

    if (ecp.element != choice_ECParameters_namedCurve) {
	ret = KRB5_BADMSGTYPE;
	goto out;
    }

    if (der_heim_oid_cmp(&ecp.u.namedCurve, &asn1_oid_id_ec_group_secp256r1) == 0)
	nid = NID_X9_62_prime256v1;
    else {
	ret = KRB5_BADMSGTYPE;
	goto out;
    }

    /* XXX verify group is ok */

    public = EC_KEY_new_by_curve_name(nid);

    p = dh_key_info->subjectPublicKey.data;
    len = dh_key_info->subjectPublicKey.length / 8;
    if (o2i_ECPublicKey(&public, &p, len) == NULL) {
	ret = KRB5_BADMSGTYPE;
	krb5_set_error_message(context, ret,
			       "PKINIT failed to decode ECDH key");
	goto out;
    }
    *out = public;
    public = NULL;

 out:
    if (public)
	EC_KEY_free(public);
    free_ECParameters(&ecp);
    return ret;
}
#endif /* HAVE_HCRYPTO_W_OPENSSL */

krb5_error_code
_kdc_get_ecdh_param(krb5_context context,
                    krb5_kdc_configuration *config,
                    SubjectPublicKeyInfo *dh_key_info,
                    void **out)
{
#ifdef HAVE_HCRYPTO_W_OPENSSL
    return get_ecdh_param(context, config, dh_key_info, (EC_KEY **)out);
#else
    return ENOTSUP;
#endif /* HAVE_HCRYPTO_W_OPENSSL */
}


/*
 *
 */

#ifdef HAVE_HCRYPTO_W_OPENSSL
static krb5_error_code
serialize_ecdh_key(krb5_context context,
                   EC_KEY *key,
                   unsigned char **out,
                   size_t *out_len)
{
    krb5_error_code ret = 0;
    unsigned char *p;
    int len;

    *out = NULL;
    *out_len = 0;

    len = i2o_ECPublicKey(key, NULL);
    if (len <= 0)
        return EOVERFLOW;

    *out = malloc(len);
    if (*out == NULL)
        return krb5_enomem(context);

    p = *out;
    len = i2o_ECPublicKey(key, &p);
    if (len <= 0) {
        free(*out);
        *out = NULL;
        ret = EINVAL; /* XXX Better error please */
	krb5_set_error_message(context, ret,
			       "PKINIT failed to encode ECDH key");
        return ret;
    }

    *out_len = len * 8;
    return ret;
}
#endif

krb5_error_code
_kdc_serialize_ecdh_key(krb5_context context,
                        void *key,
                        unsigned char **out,
                        size_t *out_len)
{
#ifdef HAVE_HCRYPTO_W_OPENSSL
    return serialize_ecdh_key(context, key, out, out_len);
#else
    return ENOTSUP;
#endif
}

#endif
