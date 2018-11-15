/*	$NetBSD: hx_locl.h,v 1.2 2017/01/28 21:31:48 christos Exp $	*/

/*
 * Copyright (c) 2004 - 2016 Kungliga Tekniska Högskolan
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

/* Id */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <assert.h>
#include <stdarg.h>
#include <err.h>
#include <limits.h>

#include <krb5/roken.h>

#include <krb5/getarg.h>
#include <krb5/base64.h>
#include <krb5/hex.h>
#include <krb5/com_err.h>
#include <krb5/parse_units.h>
#include <krb5/parse_bytes.h>

#include <krb5/krb5-types.h>

#include <krb5/rfc2459_asn1.h>
#include <krb5/cms_asn1.h>
#include <krb5/pkcs8_asn1.h>
#include <krb5/pkcs9_asn1.h>
#include <krb5/pkcs12_asn1.h>
#include <krb5/ocsp_asn1.h>
#include <krb5/pkcs10_asn1.h>
#include <krb5/asn1_err.h>
#include <krb5/pkinit_asn1.h>

#include <krb5/der.h>

/*
 * We use OpenSSL for EC, but to do this we need to disable cross-references
 * between OpenSSL and hcrypto bn.h and such.  Source files that use OpenSSL EC
 * must define HEIM_NO_CRYPTO_HDRS before including this file.
 */

#define HC_DEPRECATED_CRYPTO
#ifndef HEIM_NO_CRYPTO_HDRS
#include "crypto-headers.h"
#endif

struct hx509_keyset_ops;
struct hx509_collector;
struct hx509_generate_private_context;
typedef struct hx509_path hx509_path;

#include <krb5/heimbase.h>

#include <krb5/hx509.h>

typedef void (*_hx509_cert_release_func)(struct hx509_cert_data *, void *);


#include "sel.h"

#include <hx509-private.h>
#include <krb5/hx509_err.h>

struct hx509_peer_info {
    hx509_cert cert;
    AlgorithmIdentifier *val;
    size_t len;
};

#define HX509_CERTS_FIND_SERIALNUMBER		1
#define HX509_CERTS_FIND_ISSUER			2
#define HX509_CERTS_FIND_SUBJECT		4
#define HX509_CERTS_FIND_ISSUER_KEY_ID		8
#define HX509_CERTS_FIND_SUBJECT_KEY_ID		16

struct hx509_name_data {
    Name der_name;
};

struct hx509_path {
    size_t len;
    hx509_cert *val;
};

struct hx509_query_data {
    int match;
#define HX509_QUERY_FIND_ISSUER_CERT		0x000001
#define HX509_QUERY_MATCH_SERIALNUMBER		0x000002
#define HX509_QUERY_MATCH_ISSUER_NAME		0x000004
#define HX509_QUERY_MATCH_SUBJECT_NAME		0x000008
#define HX509_QUERY_MATCH_SUBJECT_KEY_ID	0x000010
#define HX509_QUERY_MATCH_ISSUER_ID		0x000020
#define HX509_QUERY_PRIVATE_KEY			0x000040
#define HX509_QUERY_KU_ENCIPHERMENT		0x000080
#define HX509_QUERY_KU_DIGITALSIGNATURE		0x000100
#define HX509_QUERY_KU_KEYCERTSIGN		0x000200
#define HX509_QUERY_KU_CRLSIGN			0x000400
#define HX509_QUERY_KU_NONREPUDIATION		0x000800
#define HX509_QUERY_KU_KEYAGREEMENT		0x001000
#define HX509_QUERY_KU_DATAENCIPHERMENT		0x002000
#define HX509_QUERY_ANCHOR			0x004000
#define HX509_QUERY_MATCH_CERTIFICATE		0x008000
#define HX509_QUERY_MATCH_LOCAL_KEY_ID		0x010000
#define HX509_QUERY_NO_MATCH_PATH		0x020000
#define HX509_QUERY_MATCH_FRIENDLY_NAME		0x040000
#define HX509_QUERY_MATCH_FUNCTION		0x080000
#define HX509_QUERY_MATCH_KEY_HASH_SHA1		0x100000
#define HX509_QUERY_MATCH_TIME			0x200000
#define HX509_QUERY_MATCH_EKU			0x400000
#define HX509_QUERY_MATCH_EXPR			0x800000
#define HX509_QUERY_MASK			0xffffff
    Certificate *subject;
    Certificate *certificate;
    heim_integer *serial;
    heim_octet_string *subject_id;
    heim_octet_string *local_key_id;
    Name *issuer_name;
    Name *subject_name;
    hx509_path *path;
    char *friendlyname;
    int (*cmp_func)(hx509_context, hx509_cert, void *);
    void *cmp_func_ctx;
    heim_octet_string *keyhash_sha1;
    time_t timenow;
    heim_oid *eku;
    struct hx_expr *expr;
};

struct hx509_keyset_ops {
    const char *name;
    int flags;
    int (*init)(hx509_context, hx509_certs, void **,
		int, const char *, hx509_lock);
    int (*store)(hx509_context, hx509_certs, void *, int, hx509_lock);
    int (*free)(hx509_certs, void *);
    int (*add)(hx509_context, hx509_certs, void *, hx509_cert);
    int (*query)(hx509_context, hx509_certs, void *,
		 const hx509_query *, hx509_cert *);
    int (*iter_start)(hx509_context, hx509_certs, void *, void **);
    int (*iter)(hx509_context, hx509_certs, void *, void *, hx509_cert *);
    int (*iter_end)(hx509_context, hx509_certs, void *, void *);
    int (*printinfo)(hx509_context, hx509_certs,
		     void *, int (*)(void *, const char *), void *);
    int (*getkeys)(hx509_context, hx509_certs, void *, hx509_private_key **);
    int (*addkey)(hx509_context, hx509_certs, void *, hx509_private_key);
};

struct _hx509_password {
    size_t len;
    char **val;
};

extern hx509_lock _hx509_empty_lock;

struct hx509_context_data {
    struct hx509_keyset_ops **ks_ops;
    int ks_num_ops;
    int flags;
#define HX509_CTX_VERIFY_MISSING_OK	1
    int ocsp_time_diff;
#define HX509_DEFAULT_OCSP_TIME_DIFF	(5*60)
    heim_error_t error;
    struct et_list *et_list;
    char *querystat;
    hx509_certs default_trust_anchors;
};

/* _hx509_calculate_path flag field */
#define HX509_CALCULATE_PATH_NO_ANCHOR 1

/* environment */
struct hx509_env_data {
    enum { env_string, env_list } type;
    char *name;
    struct hx509_env_data *next;
    union {
	char *string;
	struct hx509_env_data *list;
    } u;
};


extern const AlgorithmIdentifier * _hx509_crypto_default_sig_alg;
extern const AlgorithmIdentifier * _hx509_crypto_default_digest_alg;
extern const AlgorithmIdentifier * _hx509_crypto_default_secret_alg;

/*
 * Private bits from crypto.c, so crypto-ec.c can also see them.
 *
 * This is part of the use-OpenSSL-for-EC hack.
 */

struct hx509_crypto;

struct signature_alg;

struct hx509_generate_private_context {
    const heim_oid *key_oid;
    int isCA;
    unsigned long num_bits;
};

struct hx509_private_key_ops {
    const char *pemtype;
    const heim_oid *key_oid;
    int (*available)(const hx509_private_key,
		     const AlgorithmIdentifier *);
    int (*get_spki)(hx509_context,
		    const hx509_private_key,
		    SubjectPublicKeyInfo *);
    int (*export)(hx509_context context,
		  const hx509_private_key,
		  hx509_key_format_t,
		  heim_octet_string *);
    int (*import)(hx509_context, const AlgorithmIdentifier *,
		  const void *, size_t, hx509_key_format_t,
		  hx509_private_key);
    int (*generate_private_key)(hx509_context,
				struct hx509_generate_private_context *,
				hx509_private_key);
    BIGNUM *(*get_internal)(hx509_context, hx509_private_key, const char *);
};

struct hx509_private_key {
    unsigned int ref;
    const struct signature_alg *md;
    const heim_oid *signature_alg;
    union {
	RSA *rsa;
	void *keydata;
        void *ecdsa; /* EC_KEY */
    } private_key;
    hx509_private_key_ops *ops;
};

/*
 *
 */

struct signature_alg {
    const char *name;
    const heim_oid *sig_oid;
    const AlgorithmIdentifier *sig_alg;
    const heim_oid *key_oid;
    const AlgorithmIdentifier *digest_alg;
    int flags;
#define PROVIDE_CONF	0x1
#define REQUIRE_SIGNER	0x2
#define SELF_SIGNED_OK	0x4
#define WEAK_SIG_ALG	0x8

#define SIG_DIGEST	0x100
#define SIG_PUBLIC_SIG	0x200
#define SIG_SECRET	0x400

#define RA_RSA_USES_DIGEST_INFO 0x1000000

    time_t best_before; /* refuse signature made after best before date */
    const EVP_MD *(*evp_md)(void);
    int (*verify_signature)(hx509_context context,
			    const struct signature_alg *,
			    const Certificate *,
			    const AlgorithmIdentifier *,
			    const heim_octet_string *,
			    const heim_octet_string *);
    int (*create_signature)(hx509_context,
			    const struct signature_alg *,
			    const hx509_private_key,
			    const AlgorithmIdentifier *,
			    const heim_octet_string *,
			    AlgorithmIdentifier *,
			    heim_octet_string *);
    int digest_size;
};

/*
 * Configurable options
 */

#ifdef __APPLE__
#define HX509_DEFAULT_ANCHORS "KEYCHAIN:system-anchors"
#endif
