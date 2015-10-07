/* $NetBSD: crypto.c,v 1.5 2011/02/12 23:21:32 christos Exp $ */

/*
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Mateusz Kocielski.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__RCSID("$NetBSD: crypto.c,v 1.5 2011/02/12 23:21:32 christos Exp $");

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/md5.h>
#include <openssl/rand.h>

#include "crypto.h"

/**
 * @brief base64 encode data.
 * @param in input data
 * @param inlen input data length (in bytes)
 * @param out output data
 * @param outlen output data length (in bytes)
 * @return 0 on success, -1 on failure
 */
int
saslc__crypto_encode_base64(const void *in, size_t inlen,
    char **out, size_t *outlen)
{
	BIO *bio;
	BIO *b64;
	size_t enclen;
	char *r;
	int n;

	enclen = (((inlen + 2) / 3)) * 4;
	r = calloc(enclen + 1, sizeof(*r));
	if (r == NULL)
		return -1;

	if ((bio = BIO_new(BIO_s_mem())) == NULL)
		goto err;

	if ((b64 = BIO_new(BIO_f_base64())) == NULL) {
		BIO_free(bio);
		goto err;
	}
	BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
	b64 = BIO_push(b64, bio);
	if (BIO_write(b64, in, (int)inlen) != (int)inlen) {
		BIO_free_all(b64);
		goto err;
	}
	/*LINTED: no effect*/
	(void)BIO_flush(b64);
	n = BIO_read(bio, r, (int)enclen);
	BIO_free_all(b64);
	if (n < 0)
		goto err;
	if (out)
		*out = r;
	if (outlen)
		*outlen = n;
	return 0;
 err:
	free(r);
	return -1;
}

/**
 * @brief decode base64 data.
 * @param in input data
 * @param inlen input data length (in bytes)
 * @param out output data
 * @param outlen output data length (in bytes)
 * @return 0 on success, -1 on failure
 */
int
saslc__crypto_decode_base64(const char *in, size_t inlen,
    void **out, size_t *outlen)
{
	BIO *bio;
	BIO *b64;
	void *r;
	size_t declen;
	int n;

	declen = ((inlen + 3) / 4) * 3;
	r = malloc(declen + 1);
	if (r == NULL)
		return -1;

	if ((bio = BIO_new(BIO_s_mem())) == NULL)
		goto err;

	if ((b64 = BIO_new(BIO_f_base64())) == NULL) {
		BIO_free(bio);
		goto err;
	}
	BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
	b64 = BIO_push(b64, bio);
	if (BIO_write(bio, in, (int)inlen) != (int)inlen) {
		BIO_free_all(b64);
		goto err;
	}
	n = BIO_read(b64, r, (int)declen);
	BIO_free_all(b64);
	if (n < 0)
		goto err;
	((char *)r)[n] = '\0';
	if (out)
		*out = r;
	if (outlen)
		*outlen = n;
	return 0;
 err:
	free(r);
	return -1;
}

/**
 * @brief generates safe nonce basing on OpenSSL
 * RAND_pseudo_bytes, which should be enough for our purposes.
 * @param len nonce length in bytes
 * @return nonce, user is responsible for freeing nonce.
 */
char *
saslc__crypto_nonce(size_t len)
{
	char *n;

	if ((n = malloc(len)) == NULL)
		return NULL;

	if (RAND_pseudo_bytes((unsigned char *)n, (int)len) != 1) {
		free(n);
		return NULL;
	}
	return n;
}

/**
 * @brief converts MD5 binary digest into text representation.
 * @param hash MD5 digest (16 bytes) to convert
 * @return the '\0' terminated text representation of the hash.  Note
 * that user is responsible for freeing allocated memory.
 */
char *
saslc__crypto_hash_to_hex(const uint8_t *hash)
{
	static const char hex[] = "0123456789abcdef";
	char *r;
	size_t i, j;

	if ((r = malloc(MD5_DIGEST_LENGTH * 2 + 1)) == NULL)
		return NULL;

	for (i = 0; i < MD5_DIGEST_LENGTH; i++) {
		j = i * 2;
		r[j] = hex[(unsigned)hash[i] >> 4];
		r[j + 1] = hex[hash[i] & 0x0F];
	}
	r[MD5_DIGEST_LENGTH * 2] = '\0';
	return r;
}

/**
 * @brief computes md5(D)
 * @param buf input data buffer
 * @param buflen number of bytes in input data buffer
 * @param digest buffer for hash (must not be NULL)
 * @return the md5 digest, note that user is responsible for freeing
 * allocated memory if digest is not NULL.
 */
void
saslc__crypto_md5_hash(const char *buf, size_t buflen, unsigned char *digest)
{

	assert(digest != NULL);
	if (digest != NULL)
		(void)MD5((const unsigned char *)buf, buflen, digest);
}

/**
 * @brief computes md5(D)
 * @param buf input data buffer
 * @param buflen number of bytes in input data buffer
 * @return the text representation of the computed digest, note that
 * user is responsible for freeing allocated memory.
 */
char *
saslc__crypto_md5_hex(const char *buf, size_t buflen)
{
	unsigned char digest[MD5_DIGEST_LENGTH];

	(void)MD5((const unsigned char *)buf, buflen, digest);
	return saslc__crypto_hash_to_hex(digest);
}

/**
 * @brief computes hmac_md5(K, I)
 * @param key hmac_md5 key
 * @param keylen hmac_md5 key length
 * @param in input data to compute hash for
 * @param inlen input data length in bytes
 * @param hmac space for output (MD5_DIGEST_LENGTH bytes)
 * @return 0 on success, -1 on error
 */
int
saslc__crypto_hmac_md5_hash(const unsigned char *key, size_t keylen,
    const unsigned char *in, size_t inlen, unsigned char *hmac)
{
	unsigned int hmac_len;

	assert(hmac != NULL);
	if (hmac == NULL || HMAC(EVP_md5(), key, (int)keylen, in,
	    inlen, hmac, &hmac_len) == NULL)
		return -1;

	assert(hmac_len == MD5_DIGEST_LENGTH);
	return 0;
}

/**
 * @brief computes hmac_md5(K, I)
 * @param key hmac_md5 key
 * @param keylen hmac_md5 key length
 * @param in input data to compute hash for
 * @param inlen input data length in bytes
 * @return the text representation of the computed digest, note that user is
 * responsible for freeing allocated memory.
 */
char *
saslc__crypto_hmac_md5_hex(const unsigned char *key, size_t keylen,
    const unsigned char *in, size_t inlen)
{
	unsigned char digest[MD5_DIGEST_LENGTH];

	if (saslc__crypto_hmac_md5_hash(key, keylen, in, inlen, digest) == -1)
		return NULL;

	return saslc__crypto_hash_to_hex(digest);
}
