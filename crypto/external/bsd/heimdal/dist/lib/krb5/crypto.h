/*	$NetBSD: crypto.h,v 1.2 2017/01/28 21:31:49 christos Exp $	*/

/*
 * Copyright (c) 1997 - 2016 Kungliga Tekniska Högskolan
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

#ifndef HEIMDAL_SMALLER
#define DES3_OLD_ENCTYPE 1
#endif

struct _krb5_key_data {
    krb5_keyblock *key;
    krb5_data *schedule;
};

struct _krb5_key_usage;

struct krb5_crypto_data {
    struct _krb5_encryption_type *et;
    struct _krb5_key_data key;
    int num_key_usage;
    struct _krb5_key_usage *key_usage;
};

#define CRYPTO_ETYPE(C) ((C)->et->type)

/* bits for `flags' below */
#define F_KEYED			0x0001	/* checksum is keyed */
#define F_CPROOF		0x0002	/* checksum is collision proof */
#define F_DERIVED		0x0004	/* uses derived keys */
#define F_VARIANT		0x0008	/* uses `variant' keys (6.4.3) */
#define F_PSEUDO		0x0010	/* not a real protocol type */
#define F_DISABLED		0x0020	/* enctype/checksum disabled */
#define F_WEAK			0x0040	/* enctype is considered weak */

#define F_RFC3961_ENC		0x0100	/* RFC3961 simplified profile */
#define F_SPECIAL		0x0200	/* backwards */
#define F_ENC_THEN_CKSUM	0x0400  /* checksum is over encrypted data */
#define F_CRYPTO_MASK		0x0F00

#define F_RFC3961_KDF		0x1000	/* RFC3961 KDF */
#define F_SP800_108_HMAC_KDF	0x2000	/* SP800-108 HMAC KDF */
#define F_KDF_MASK		0xF000

struct salt_type {
    krb5_salttype type;
    const char *name;
    krb5_error_code (*string_to_key)(krb5_context, krb5_enctype, krb5_data,
				     krb5_salt, krb5_data, krb5_keyblock*);
};

struct _krb5_key_type {
    krb5_enctype type;
    const char *name;
    size_t bits;
    size_t size;
    size_t schedule_size;
    void (*random_key)(krb5_context, krb5_keyblock*);
    void (*schedule)(krb5_context, struct _krb5_key_type *, struct _krb5_key_data *);
    struct salt_type *string_to_key;
    void (*random_to_key)(krb5_context, krb5_keyblock*, const void*, size_t);
    void (*cleanup)(krb5_context, struct _krb5_key_data *);
    const EVP_CIPHER *(*evp)(void);
};

struct _krb5_checksum_type {
    krb5_cksumtype type;
    const char *name;
    size_t blocksize;
    size_t checksumsize;
    unsigned flags;
    krb5_error_code (*checksum)(krb5_context context,
				struct _krb5_key_data *key,
				const void *buf, size_t len,
				unsigned usage,
				Checksum *csum);
    krb5_error_code (*verify)(krb5_context context,
			      struct _krb5_key_data *key,
			      const void *buf, size_t len,
			      unsigned usage,
			      Checksum *csum);
};

struct _krb5_encryption_type {
    krb5_enctype type;
    const char *name;
    const char *alias;
    size_t blocksize;
    size_t padsize;
    size_t confoundersize;
    struct _krb5_key_type *keytype;
    struct _krb5_checksum_type *checksum;
    struct _krb5_checksum_type *keyed_checksum;
    unsigned flags;
    krb5_error_code (*encrypt)(krb5_context context,
			       struct _krb5_key_data *key,
			       void *data, size_t len,
			       krb5_boolean encryptp,
			       int usage,
			       void *ivec);
    size_t prf_length;
    krb5_error_code (*prf)(krb5_context,
			   krb5_crypto, const krb5_data *, krb5_data *);
};

#define ENCRYPTION_USAGE(U) (((U) << 8) | 0xAA)
#define INTEGRITY_USAGE(U) (((U) << 8) | 0x55)
#define CHECKSUM_USAGE(U) (((U) << 8) | 0x99)

/* Checksums */

extern struct _krb5_checksum_type _krb5_checksum_none;
extern struct _krb5_checksum_type _krb5_checksum_crc32;
extern struct _krb5_checksum_type _krb5_checksum_rsa_md4;
extern struct _krb5_checksum_type _krb5_checksum_rsa_md4_des;
extern struct _krb5_checksum_type _krb5_checksum_rsa_md5_des;
extern struct _krb5_checksum_type _krb5_checksum_rsa_md5_des3;
extern struct _krb5_checksum_type _krb5_checksum_rsa_md5;
extern struct _krb5_checksum_type _krb5_checksum_hmac_sha1_des3;
extern struct _krb5_checksum_type _krb5_checksum_hmac_sha1_aes128;
extern struct _krb5_checksum_type _krb5_checksum_hmac_sha1_aes256;
extern struct _krb5_checksum_type _krb5_checksum_hmac_sha256_128_aes128;
extern struct _krb5_checksum_type _krb5_checksum_hmac_sha384_192_aes256;
extern struct _krb5_checksum_type _krb5_checksum_hmac_md5;
extern struct _krb5_checksum_type _krb5_checksum_sha1;
extern struct _krb5_checksum_type _krb5_checksum_sha2;

extern struct _krb5_checksum_type *_krb5_checksum_types[];
extern int _krb5_num_checksums;

/* Salts */

extern struct salt_type _krb5_AES_SHA1_salt[];
extern struct salt_type _krb5_AES_SHA2_salt[];
extern struct salt_type _krb5_arcfour_salt[];
extern struct salt_type _krb5_des_salt[];
extern struct salt_type _krb5_des3_salt[];
extern struct salt_type _krb5_des3_salt_derived[];

/* Encryption types */

extern struct _krb5_encryption_type _krb5_enctype_aes256_cts_hmac_sha1;
extern struct _krb5_encryption_type _krb5_enctype_aes128_cts_hmac_sha1;
extern struct _krb5_encryption_type _krb5_enctype_aes128_cts_hmac_sha256_128;
extern struct _krb5_encryption_type _krb5_enctype_aes256_cts_hmac_sha384_192;
extern struct _krb5_encryption_type _krb5_enctype_des3_cbc_sha1;
extern struct _krb5_encryption_type _krb5_enctype_des3_cbc_md5;
extern struct _krb5_encryption_type _krb5_enctype_des3_cbc_none;
extern struct _krb5_encryption_type _krb5_enctype_arcfour_hmac_md5;
extern struct _krb5_encryption_type _krb5_enctype_des_cbc_md5;
extern struct _krb5_encryption_type _krb5_enctype_old_des3_cbc_sha1;
extern struct _krb5_encryption_type _krb5_enctype_des_cbc_crc;
extern struct _krb5_encryption_type _krb5_enctype_des_cbc_md4;
extern struct _krb5_encryption_type _krb5_enctype_des_cbc_md5;
extern struct _krb5_encryption_type _krb5_enctype_des_cbc_none;
extern struct _krb5_encryption_type _krb5_enctype_des_cfb64_none;
extern struct _krb5_encryption_type _krb5_enctype_des_pcbc_none;
extern struct _krb5_encryption_type _krb5_enctype_null;

extern struct _krb5_encryption_type *_krb5_etypes[];
extern int _krb5_num_etypes;

/* NO_HCRYPTO_POLLUTION is defined in pkinit-ec.c.  See commentary there. */
#ifndef NO_HCRYPTO_POLLUTION
/* Interface to the EVP crypto layer provided by hcrypto */
struct _krb5_evp_schedule {
    /*
     * Normally we'd say EVP_CIPHER_CTX here, but!  this header gets
     * included in lib/krb5/pkinit-ec.ck
     */
    EVP_CIPHER_CTX ectx;
    EVP_CIPHER_CTX dctx;
};
#endif
