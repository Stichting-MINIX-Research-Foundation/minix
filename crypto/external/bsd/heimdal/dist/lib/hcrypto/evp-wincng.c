/*	$NetBSD: evp-wincng.c,v 1.2 2017/01/28 21:31:47 christos Exp $	*/

/*
 * Copyright (c) 2015, Secure Endpoints Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Windows CNG provider */

#include <config.h>
#include <krb5/roken.h>
#include <assert.h>

#include <evp.h>
#include <evp-wincng.h>

#include <bcrypt.h>

/*
 * CNG cipher provider
 */

struct wincng_key {
    BCRYPT_KEY_HANDLE hKey;
    UCHAR rgbKeyObject[1];
};

#define WINCNG_KEY_OBJECT_SIZE(ctx) \
	((ctx)->cipher->ctx_size - sizeof(struct wincng_key) + 1)

static int
wincng_do_cipher(EVP_CIPHER_CTX *ctx,
		 unsigned char *out,
		 const unsigned char *in,
		 unsigned int size)
{
    struct wincng_key *cng = ctx->cipher_data;
    NTSTATUS status;
    ULONG cbResult;

    assert(EVP_CIPHER_CTX_mode(ctx) == EVP_CIPH_STREAM_CIPHER ||
	   (size % ctx->cipher->block_size) == 0);

    if (ctx->encrypt) {
	status = BCryptEncrypt(cng->hKey,
			       (PUCHAR)in,
			       size,
			       NULL, /* pPaddingInfo */
			       ctx->cipher->iv_len ? ctx->iv : NULL,
			       ctx->cipher->iv_len,
			       out,
			       size,
			       &cbResult,
			       0);
    } else {
	status = BCryptDecrypt(cng->hKey,
			       (PUCHAR)in,
			       size,
			       NULL, /* pPaddingInfo */
			       ctx->cipher->iv_len ? ctx->iv : NULL,
			       ctx->cipher->iv_len,
			       out,
			       size,
			       &cbResult,
			       0);
    }

    return BCRYPT_SUCCESS(status) && cbResult == size;
}

static int
wincng_cleanup(EVP_CIPHER_CTX *ctx)
{
    struct wincng_key *cng = ctx->cipher_data;

    if (cng->hKey)
	BCryptDestroyKey(cng->hKey);
    SecureZeroMemory(cng->rgbKeyObject, WINCNG_KEY_OBJECT_SIZE(ctx));

    return 1;
}

static int
wincng_cipher_algorithm_init(EVP_CIPHER *cipher,
			     LPWSTR pszAlgId)
{
    BCRYPT_ALG_HANDLE hAlgorithm = NULL;
    NTSTATUS status;
    LPCWSTR pszChainingMode;
    ULONG cbKeyObject, cbChainingMode, cbData;

    if (cipher->app_data)
	return 1;

    status = BCryptOpenAlgorithmProvider(&hAlgorithm,
					 pszAlgId,
					 NULL,
					 0);
    if (!BCRYPT_SUCCESS(status))
	return 0;

    status = BCryptGetProperty(hAlgorithm,
			       BCRYPT_OBJECT_LENGTH,
			       (PUCHAR)&cbKeyObject,
			       sizeof(ULONG),
			       &cbData,
			       0);
    if (!BCRYPT_SUCCESS(status)) {
	BCryptCloseAlgorithmProvider(hAlgorithm, 0);
	return 0;
    }

    cipher->ctx_size = sizeof(struct wincng_key) + cbKeyObject - 1;

    switch (cipher->flags & EVP_CIPH_MODE) {
    case EVP_CIPH_CBC_MODE:
	pszChainingMode = BCRYPT_CHAIN_MODE_CBC;
	cbChainingMode = sizeof(BCRYPT_CHAIN_MODE_CBC);
	break;
    case EVP_CIPH_CFB8_MODE:
	pszChainingMode = BCRYPT_CHAIN_MODE_CFB;
	cbChainingMode = sizeof(BCRYPT_CHAIN_MODE_CFB);
	break;
    default:
	pszChainingMode = NULL;
	cbChainingMode = 0;
	break;
    }

    if (cbChainingMode) {
	status = BCryptSetProperty(hAlgorithm,
				   BCRYPT_CHAINING_MODE,
				   (PUCHAR)pszChainingMode,
				   cbChainingMode,
				   0);
	if (!BCRYPT_SUCCESS(status)) {
	    BCryptCloseAlgorithmProvider(hAlgorithm, 0);
	    return 0;
	}
    }

    if (wcscmp(pszAlgId, BCRYPT_RC2_ALGORITHM) == 0) {
	ULONG cbEffectiveKeyLength = EVP_CIPHER_key_length(cipher) * 8;

	status = BCryptSetProperty(hAlgorithm,
				   BCRYPT_EFFECTIVE_KEY_LENGTH,
				   (PUCHAR)&cbEffectiveKeyLength,
				   sizeof(cbEffectiveKeyLength),
				   0);
	if (!BCRYPT_SUCCESS(status)) {
	    BCryptCloseAlgorithmProvider(hAlgorithm, 0);
	    return 0;
	}
    }

    InterlockedCompareExchangePointerRelease(&cipher->app_data,
					     hAlgorithm, NULL);
    return 1;
}

static int
wincng_key_init(EVP_CIPHER_CTX *ctx,
		const unsigned char *key,
		const unsigned char *iv,
		int encp)
{
    struct wincng_key *cng = ctx->cipher_data;
    NTSTATUS status;

    assert(cng != NULL);
    assert(ctx->cipher != NULL);

    if (ctx->cipher->app_data == NULL)
	return 0;

    /*
     * Note: ctx->key_len not EVP_CIPHER_CTX_key_length() for
     * variable length key support.
     */
    status = BCryptGenerateSymmetricKey(ctx->cipher->app_data,
					&cng->hKey,
					cng->rgbKeyObject,
					WINCNG_KEY_OBJECT_SIZE(ctx),
					(PUCHAR)key,
					ctx->key_len,
					0);

    return BCRYPT_SUCCESS(status);
}

#define WINCNG_CIPHER_ALGORITHM(name, alg_id, block_size, key_len,      \
				iv_len, flags)				\
									\
    static EVP_CIPHER							\
    wincng_##name = {							\
	0,								\
	block_size,							\
	key_len,							\
	iv_len,								\
	flags,								\
	wincng_key_init,						\
	wincng_do_cipher,						\
	wincng_cleanup,							\
	0,								\
	NULL,								\
	NULL,								\
	NULL,								\
	NULL								\
    };									\
									\
    const EVP_CIPHER *							\
    hc_EVP_wincng_##name(void)						\
    {									\
	wincng_cipher_algorithm_init(&wincng_##name, alg_id);		\
	return wincng_##name.app_data ? &wincng_##name : NULL;		\
    }

#define WINCNG_CIPHER_ALGORITHM_CLEANUP(name) do {			\
	if (wincng_##name.app_data) {					\
	    BCryptCloseAlgorithmProvider(wincng_##name.app_data, 0);	\
	    wincng_##name.app_data = NULL;				\
	}								\
    } while (0)

#define WINCNG_CIPHER_ALGORITHM_UNAVAILABLE(name)			\
									\
    const EVP_CIPHER *							\
    hc_EVP_wincng_##name(void)						\
    {									\
	return NULL;							\
    }

/**
 * The triple DES cipher type (Windows CNG provider)
 *
 * @return the DES-EDE3-CBC EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

WINCNG_CIPHER_ALGORITHM(des_ede3_cbc,
			BCRYPT_3DES_ALGORITHM,
			8,
			24,
			8,
			EVP_CIPH_CBC_MODE);

/**
 * The DES cipher type (Windows CNG provider)
 *
 * @return the DES-CBC EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

WINCNG_CIPHER_ALGORITHM(des_cbc,
			BCRYPT_DES_ALGORITHM,
			8,
			8,
			8,
			EVP_CIPH_CBC_MODE);

/**
 * The AES-128 cipher type (Windows CNG provider)
 *
 * @return the AES-128-CBC EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

WINCNG_CIPHER_ALGORITHM(aes_128_cbc,
			BCRYPT_AES_ALGORITHM,
			16,
			16,
			16,
			EVP_CIPH_CBC_MODE);

/**
 * The AES-192 cipher type (Windows CNG provider)
 *
 * @return the AES-192-CBC EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

WINCNG_CIPHER_ALGORITHM(aes_192_cbc,
			BCRYPT_AES_ALGORITHM,
			16,
			24,
			16,
			EVP_CIPH_CBC_MODE);

/**
 * The AES-256 cipher type (Windows CNG provider)
 *
 * @return the AES-256-CBC EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

WINCNG_CIPHER_ALGORITHM(aes_256_cbc,
			BCRYPT_AES_ALGORITHM,
			16,
			32,
			16,
			EVP_CIPH_CBC_MODE);

/**
 * The AES-128 CFB8 cipher type (Windows CNG provider)
 *
 * @return the AES-128-CFB8 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

WINCNG_CIPHER_ALGORITHM(aes_128_cfb8,
			BCRYPT_AES_ALGORITHM,
			16,
			16,
			16,
			EVP_CIPH_CFB8_MODE);

/**
 * The AES-192 CFB8 cipher type (Windows CNG provider)
 *
 * @return the AES-192-CFB8 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

WINCNG_CIPHER_ALGORITHM(aes_192_cfb8,
			BCRYPT_AES_ALGORITHM,
			16,
			24,
			16,
			EVP_CIPH_CFB8_MODE);

/**
 * The AES-256 CFB8 cipher type (Windows CNG provider)
 *
 * @return the AES-256-CFB8 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

WINCNG_CIPHER_ALGORITHM(aes_256_cfb8,
			BCRYPT_AES_ALGORITHM,
			16,
			32,
			16,
			EVP_CIPH_CFB8_MODE);

/**
 * The RC2 cipher type - Windows CNG
 *
 * @return the RC2 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

WINCNG_CIPHER_ALGORITHM(rc2_cbc,
			BCRYPT_RC2_ALGORITHM,
			8,
			16,
			8,
			EVP_CIPH_CBC_MODE);

/**
 * The RC2-40 cipher type - Windows CNG
 *
 * @return the RC2-40 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

WINCNG_CIPHER_ALGORITHM(rc2_40_cbc,
			BCRYPT_RC2_ALGORITHM,
			8,
			5,
			8,
			EVP_CIPH_CBC_MODE);

/**
 * The RC2-64 cipher type - Windows CNG
 *
 * @return the RC2-64 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

WINCNG_CIPHER_ALGORITHM(rc2_64_cbc,
			BCRYPT_RC2_ALGORITHM,
			8,
			8,
			8,
			EVP_CIPH_CBC_MODE);

/**
 * The Camellia-128 cipher type - CommonCrypto
 *
 * @return the Camellia-128 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

WINCNG_CIPHER_ALGORITHM_UNAVAILABLE(camellia_128_cbc);

/**
 * The Camellia-198 cipher type - CommonCrypto
 *
 * @return the Camellia-198 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

WINCNG_CIPHER_ALGORITHM_UNAVAILABLE(camellia_192_cbc);

/**
 * The Camellia-256 cipher type - CommonCrypto
 *
 * @return the Camellia-256 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

WINCNG_CIPHER_ALGORITHM_UNAVAILABLE(camellia_256_cbc);

/**
 * The RC4 cipher type (Windows CNG provider)
 *
 * @return the RC4 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

WINCNG_CIPHER_ALGORITHM(rc4,
			BCRYPT_RC4_ALGORITHM,
			1,
			16,
			0,
			EVP_CIPH_STREAM_CIPHER | EVP_CIPH_VARIABLE_LENGTH);

/**
 * The RC4-40 cipher type (Windows CNG provider)
 *
 * @return the RC4 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

WINCNG_CIPHER_ALGORITHM(rc4_40,
			BCRYPT_RC4_ALGORITHM,
			1,
			5,
			0,
			EVP_CIPH_STREAM_CIPHER | EVP_CIPH_VARIABLE_LENGTH);

static void
wincng_cipher_algorithm_cleanup(void)
{
    WINCNG_CIPHER_ALGORITHM_CLEANUP(des_ede3_cbc);
    WINCNG_CIPHER_ALGORITHM_CLEANUP(des_cbc);
    WINCNG_CIPHER_ALGORITHM_CLEANUP(aes_128_cbc);
    WINCNG_CIPHER_ALGORITHM_CLEANUP(aes_192_cbc);
    WINCNG_CIPHER_ALGORITHM_CLEANUP(aes_256_cbc);
    WINCNG_CIPHER_ALGORITHM_CLEANUP(aes_128_cfb8);
    WINCNG_CIPHER_ALGORITHM_CLEANUP(aes_192_cfb8);
    WINCNG_CIPHER_ALGORITHM_CLEANUP(aes_256_cfb8);
    WINCNG_CIPHER_ALGORITHM_CLEANUP(rc2_cbc);
    WINCNG_CIPHER_ALGORITHM_CLEANUP(rc2_40_cbc);
    WINCNG_CIPHER_ALGORITHM_CLEANUP(rc2_64_cbc);
    WINCNG_CIPHER_ALGORITHM_CLEANUP(rc4);
    WINCNG_CIPHER_ALGORITHM_CLEANUP(rc4_40);
}

/*
 * CNG digest provider
 */

struct wincng_md_ctx {
    BCRYPT_HASH_HANDLE hHash;
    ULONG cbHashObject;
    UCHAR rgbHashObject[1];
};

static BCRYPT_ALG_HANDLE
wincng_md_algorithm_init(EVP_MD *md,
			 LPCWSTR pszAlgId)
{
    BCRYPT_ALG_HANDLE hAlgorithm;
    NTSTATUS status;
    ULONG cbHashObject, cbData;
    ULONG cbHash = 0, cbBlock = 0;

    status = BCryptOpenAlgorithmProvider(&hAlgorithm,
					 pszAlgId,
					 NULL,
					 0);
    if (!BCRYPT_SUCCESS(status))
	return NULL;

    status = BCryptGetProperty(hAlgorithm,
			       BCRYPT_HASH_LENGTH,
			       (PUCHAR)&cbHash,
			       sizeof(ULONG),
			       &cbData,
			       0);
    if (!BCRYPT_SUCCESS(status)) {
	BCryptCloseAlgorithmProvider(hAlgorithm, 0);
	return NULL;
    }

    status = BCryptGetProperty(hAlgorithm,
			       BCRYPT_HASH_BLOCK_LENGTH,
			       (PUCHAR)&cbBlock,
			       sizeof(ULONG),
			       &cbData,
			       0);
    if (!BCRYPT_SUCCESS(status)) {
	BCryptCloseAlgorithmProvider(hAlgorithm, 0);
	return NULL;
    }

    status = BCryptGetProperty(hAlgorithm,
			       BCRYPT_OBJECT_LENGTH,
			       (PUCHAR)&cbHashObject,
			       sizeof(ULONG),
			       &cbData,
			       0);
    if (!BCRYPT_SUCCESS(status)) {
	BCryptCloseAlgorithmProvider(hAlgorithm, 0);
	return NULL;
    }

    md->hash_size = cbHash;
    md->block_size = cbBlock;
    md->ctx_size = sizeof(struct wincng_md_ctx) + cbHashObject - 1;

    return hAlgorithm;
}

static int
wincng_md_hash_init(BCRYPT_ALG_HANDLE hAlgorithm,
		    EVP_MD_CTX *ctx)
{
    struct wincng_md_ctx *cng = (struct wincng_md_ctx *)ctx;
    NTSTATUS status;
    ULONG cbData;

    status = BCryptGetProperty(hAlgorithm,
			       BCRYPT_OBJECT_LENGTH,
			       (PUCHAR)&cng->cbHashObject,
			       sizeof(ULONG),
			       &cbData,
			       0);
    if (!BCRYPT_SUCCESS(status))
	return 0;

    status = BCryptCreateHash(hAlgorithm,
			      &cng->hHash,
			      cng->rgbHashObject,
			      cng->cbHashObject,
			      NULL,
			      0,
			      0);

    return BCRYPT_SUCCESS(status);
}

static int
wincng_md_update(EVP_MD_CTX *ctx,
		 const void *data,
		 size_t length)
{
    struct wincng_md_ctx *cng = (struct wincng_md_ctx *)ctx;
    NTSTATUS status;

    status = BCryptHashData(cng->hHash, (PUCHAR)data, length, 0);

    return BCRYPT_SUCCESS(status);
}

static int
wincng_md_final(void *digest,
		EVP_MD_CTX *ctx)
{
    struct wincng_md_ctx *cng = (struct wincng_md_ctx *)ctx;
    NTSTATUS status;
    ULONG cbHash, cbData;

    status = BCryptGetProperty(cng->hHash,
			       BCRYPT_HASH_LENGTH,
			       (PUCHAR)&cbHash,
			       sizeof(DWORD),
			       &cbData,
			       0);
    if (!BCRYPT_SUCCESS(status))
	return 0;

    status = BCryptFinishHash(cng->hHash,
			      digest,
			      cbHash,
			      0);

    return BCRYPT_SUCCESS(status);
}

static int
wincng_md_cleanup(EVP_MD_CTX *ctx)
{
    struct wincng_md_ctx *cng = (struct wincng_md_ctx *)ctx;

    if (cng->hHash)
	BCryptDestroyHash(cng->hHash);
    SecureZeroMemory(cng->rgbHashObject, cng->cbHashObject);

    return 1;
}

#define WINCNG_MD_ALGORITHM(name, alg_id)				\
									\
    static BCRYPT_ALG_HANDLE wincng_hAlgorithm_##name;			\
									\
    static int wincng_##name##_init(EVP_MD_CTX *ctx)			\
    {									\
	return wincng_md_hash_init(wincng_hAlgorithm_##name, ctx);	\
    }									\
									\
    const EVP_MD *							\
    hc_EVP_wincng_##name(void)						\
    {									\
	static struct hc_evp_md name = {				\
	    0,								\
	    0,								\
	    0,								\
	    wincng_##name##_init,					\
	    wincng_md_update,						\
	    wincng_md_final,						\
	    wincng_md_cleanup						\
	};								\
									\
	if (wincng_hAlgorithm_##name == NULL) {				\
	    BCRYPT_ALG_HANDLE hAlgorithm =				\
		wincng_md_algorithm_init(&name, alg_id);		\
	    InterlockedCompareExchangePointerRelease(			\
		&wincng_hAlgorithm_##name, hAlgorithm, NULL);		\
	}								\
	return wincng_hAlgorithm_##name ? &name : NULL;			\
    }

#define WINCNG_MD_ALGORITHM_CLEANUP(name) do {				\
	if (wincng_hAlgorithm_##name) {					\
	    BCryptCloseAlgorithmProvider(wincng_hAlgorithm_##name, 0);	\
	    wincng_hAlgorithm_##name = NULL;				\
	}								\
    } while (0)

WINCNG_MD_ALGORITHM(md2,    BCRYPT_MD2_ALGORITHM);
WINCNG_MD_ALGORITHM(md4,    BCRYPT_MD4_ALGORITHM);
WINCNG_MD_ALGORITHM(md5,    BCRYPT_MD5_ALGORITHM);
WINCNG_MD_ALGORITHM(sha1,   BCRYPT_SHA1_ALGORITHM);
WINCNG_MD_ALGORITHM(sha256, BCRYPT_SHA256_ALGORITHM);
WINCNG_MD_ALGORITHM(sha384, BCRYPT_SHA384_ALGORITHM);
WINCNG_MD_ALGORITHM(sha512, BCRYPT_SHA512_ALGORITHM);

static void
wincng_md_algorithm_cleanup(void)
{
    WINCNG_MD_ALGORITHM_CLEANUP(md2);
    WINCNG_MD_ALGORITHM_CLEANUP(md4);
    WINCNG_MD_ALGORITHM_CLEANUP(md5);
    WINCNG_MD_ALGORITHM_CLEANUP(sha1);
    WINCNG_MD_ALGORITHM_CLEANUP(sha256);
    WINCNG_MD_ALGORITHM_CLEANUP(sha384);
    WINCNG_MD_ALGORITHM_CLEANUP(sha512);
}

void _hc_wincng_cleanup(void)
{
    wincng_md_algorithm_cleanup();
    wincng_cipher_algorithm_cleanup();
}
