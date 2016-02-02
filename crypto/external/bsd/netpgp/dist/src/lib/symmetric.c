/*-
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Alistair Crooks (agc@NetBSD.org)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
/*
 * Copyright (c) 2005-2008 Nominet UK (www.nic.uk)
 * All rights reserved.
 * Contributors: Ben Laurie, Rachel Willmer. The Contributors have asserted
 * their moral rights under the UK Copyright Design and Patents Act 1988 to
 * be recorded as the authors of this copyright work.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "config.h"

#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif

#if defined(__NetBSD__)
__COPYRIGHT("@(#) Copyright (c) 2009 The NetBSD Foundation, Inc. All rights reserved.");
__RCSID("$NetBSD: symmetric.c,v 1.18 2010/11/07 08:39:59 agc Exp $");
#endif

#include "crypto.h"
#include "packet-show.h"

#include <string.h>

#ifdef HAVE_OPENSSL_CAST_H
#include <openssl/cast.h>
#endif

#ifdef HAVE_OPENSSL_IDEA_H
#include <openssl/idea.h>
#endif

#ifdef HAVE_OPENSSL_AES_H
#include <openssl/aes.h>
#endif

#ifdef HAVE_OPENSSL_DES_H
#include <openssl/des.h>
#endif

#ifdef HAVE_OPENSSL_CAMELLIA_H
#include <openssl/camellia.h>
#endif

#include "crypto.h"
#include "netpgpdefs.h"


static void 
std_set_iv(pgp_crypt_t *crypt, const uint8_t *iv)
{
	(void) memcpy(crypt->iv, iv, crypt->blocksize);
	crypt->num = 0;
}

static void 
std_set_key(pgp_crypt_t *crypt, const uint8_t *key)
{
	(void) memcpy(crypt->key, key, crypt->keysize);
}

static void 
std_resync(pgp_crypt_t *decrypt)
{
	if ((size_t) decrypt->num == decrypt->blocksize) {
		return;
	}

	memmove(decrypt->civ + decrypt->blocksize - decrypt->num, decrypt->civ,
		(unsigned)decrypt->num);
	(void) memcpy(decrypt->civ, decrypt->siv + decrypt->num,
	       decrypt->blocksize - decrypt->num);
	decrypt->num = 0;
}

static void 
std_finish(pgp_crypt_t *crypt)
{
	if (crypt->encrypt_key) {
		free(crypt->encrypt_key);
		crypt->encrypt_key = NULL;
	}
	if (crypt->decrypt_key) {
		free(crypt->decrypt_key);
		crypt->decrypt_key = NULL;
	}
}

static int 
cast5_init(pgp_crypt_t *crypt)
{
	if (crypt->encrypt_key) {
		free(crypt->encrypt_key);
	}
	if ((crypt->encrypt_key = calloc(1, sizeof(CAST_KEY))) == NULL) {
		(void) fprintf(stderr, "cast5_init: alloc failure\n");
		return 0;
	}
	CAST_set_key(crypt->encrypt_key, (int)crypt->keysize, crypt->key);
	if ((crypt->decrypt_key = calloc(1, sizeof(CAST_KEY))) == NULL) {
		(void) fprintf(stderr, "cast5_init: alloc failure\n");
		return 0;
	}
	CAST_set_key(crypt->decrypt_key, (int)crypt->keysize, crypt->key);
	return 1;
}

static void 
cast5_block_encrypt(pgp_crypt_t *crypt, void *out, const void *in)
{
	CAST_ecb_encrypt(in, out, crypt->encrypt_key, CAST_ENCRYPT);
}

static void 
cast5_block_decrypt(pgp_crypt_t *crypt, void *out, const void *in)
{
	CAST_ecb_encrypt(in, out, crypt->encrypt_key, CAST_DECRYPT);
}

static void 
cast5_cfb_encrypt(pgp_crypt_t *crypt, void *out, const void *in, size_t count)
{
	CAST_cfb64_encrypt(in, out, (long)count,
			   crypt->encrypt_key, crypt->iv, &crypt->num,
			   CAST_ENCRYPT);
}

static void 
cast5_cfb_decrypt(pgp_crypt_t *crypt, void *out, const void *in, size_t count)
{
	CAST_cfb64_encrypt(in, out, (long)count,
			   crypt->encrypt_key, crypt->iv, &crypt->num,
			   CAST_DECRYPT);
}

#define TRAILER		"","","","",0,NULL,NULL

static pgp_crypt_t cast5 =
{
	PGP_SA_CAST5,
	CAST_BLOCK,
	CAST_KEY_LENGTH,
	std_set_iv,
	std_set_key,
	cast5_init,
	std_resync,
	cast5_block_encrypt,
	cast5_block_decrypt,
	cast5_cfb_encrypt,
	cast5_cfb_decrypt,
	std_finish,
	TRAILER
};

#ifndef OPENSSL_NO_IDEA
static int 
idea_init(pgp_crypt_t *crypt)
{
	if (crypt->keysize != IDEA_KEY_LENGTH) {
		(void) fprintf(stderr, "idea_init: keysize wrong\n");
		return 0;
	}

	if (crypt->encrypt_key) {
		free(crypt->encrypt_key);
	}
	if ((crypt->encrypt_key = calloc(1, sizeof(IDEA_KEY_SCHEDULE))) == NULL) {
		(void) fprintf(stderr, "idea_init: alloc failure\n");
		return 0;
	}

	/* note that we don't invert the key when decrypting for CFB mode */
	idea_set_encrypt_key(crypt->key, crypt->encrypt_key);

	if (crypt->decrypt_key) {
		free(crypt->decrypt_key);
	}
	if ((crypt->decrypt_key = calloc(1, sizeof(IDEA_KEY_SCHEDULE))) == NULL) {
		(void) fprintf(stderr, "idea_init: alloc failure\n");
		return 0;
	}

	idea_set_decrypt_key(crypt->encrypt_key, crypt->decrypt_key);
	return 1;
}

static void 
idea_block_encrypt(pgp_crypt_t *crypt, void *out, const void *in)
{
	idea_ecb_encrypt(in, out, crypt->encrypt_key);
}

static void 
idea_block_decrypt(pgp_crypt_t *crypt, void *out, const void *in)
{
	idea_ecb_encrypt(in, out, crypt->decrypt_key);
}

static void 
idea_cfb_encrypt(pgp_crypt_t *crypt, void *out, const void *in, size_t count)
{
	idea_cfb64_encrypt(in, out, (long)count,
			   crypt->encrypt_key, crypt->iv, &crypt->num,
			   CAST_ENCRYPT);
}

static void 
idea_cfb_decrypt(pgp_crypt_t *crypt, void *out, const void *in, size_t count)
{
	idea_cfb64_encrypt(in, out, (long)count,
			   crypt->decrypt_key, crypt->iv, &crypt->num,
			   CAST_DECRYPT);
}

static const pgp_crypt_t idea =
{
	PGP_SA_IDEA,
	IDEA_BLOCK,
	IDEA_KEY_LENGTH,
	std_set_iv,
	std_set_key,
	idea_init,
	std_resync,
	idea_block_encrypt,
	idea_block_decrypt,
	idea_cfb_encrypt,
	idea_cfb_decrypt,
	std_finish,
	TRAILER
};
#endif				/* OPENSSL_NO_IDEA */

/* AES with 128-bit key (AES) */

#define KEYBITS_AES128 128

static int 
aes128_init(pgp_crypt_t *crypt)
{
	if (crypt->encrypt_key) {
		free(crypt->encrypt_key);
	}
	if ((crypt->encrypt_key = calloc(1, sizeof(AES_KEY))) == NULL) {
		(void) fprintf(stderr, "aes128_init: alloc failure\n");
		return 0;
	}
	if (AES_set_encrypt_key(crypt->key, KEYBITS_AES128,
			crypt->encrypt_key)) {
		fprintf(stderr, "aes128_init: Error setting encrypt_key\n");
	}

	if (crypt->decrypt_key) {
		free(crypt->decrypt_key);
	}
	if ((crypt->decrypt_key = calloc(1, sizeof(AES_KEY))) == NULL) {
		(void) fprintf(stderr, "aes128_init: alloc failure\n");
		return 0;
	}
	if (AES_set_decrypt_key(crypt->key, KEYBITS_AES128,
				crypt->decrypt_key)) {
		fprintf(stderr, "aes128_init: Error setting decrypt_key\n");
	}
	return 1;
}

static void 
aes_block_encrypt(pgp_crypt_t *crypt, void *out, const void *in)
{
	AES_encrypt(in, out, crypt->encrypt_key);
}

static void 
aes_block_decrypt(pgp_crypt_t *crypt, void *out, const void *in)
{
	AES_decrypt(in, out, crypt->decrypt_key);
}

static void 
aes_cfb_encrypt(pgp_crypt_t *crypt, void *out, const void *in, size_t count)
{
	AES_cfb128_encrypt(in, out, (unsigned)count,
			   crypt->encrypt_key, crypt->iv, &crypt->num,
			   AES_ENCRYPT);
}

static void 
aes_cfb_decrypt(pgp_crypt_t *crypt, void *out, const void *in, size_t count)
{
	AES_cfb128_encrypt(in, out, (unsigned)count,
			   crypt->encrypt_key, crypt->iv, &crypt->num,
			   AES_DECRYPT);
}

static const pgp_crypt_t aes128 =
{
	PGP_SA_AES_128,
	AES_BLOCK_SIZE,
	KEYBITS_AES128 / 8,
	std_set_iv,
	std_set_key,
	aes128_init,
	std_resync,
	aes_block_encrypt,
	aes_block_decrypt,
	aes_cfb_encrypt,
	aes_cfb_decrypt,
	std_finish,
	TRAILER
};

/* AES with 256-bit key */

#define KEYBITS_AES256 256

static int 
aes256_init(pgp_crypt_t *crypt)
{
	if (crypt->encrypt_key) {
		free(crypt->encrypt_key);
	}
	if ((crypt->encrypt_key = calloc(1, sizeof(AES_KEY))) == NULL) {
		(void) fprintf(stderr, "aes256_init: alloc failure\n");
		return 0;
	}
	if (AES_set_encrypt_key(crypt->key, KEYBITS_AES256,
			crypt->encrypt_key)) {
		fprintf(stderr, "aes256_init: Error setting encrypt_key\n");
		free(crypt->encrypt_key);
		crypt->encrypt_key = NULL;
		return 0;
	}
	if (crypt->decrypt_key) {
		free(crypt->decrypt_key);
	}
	if ((crypt->decrypt_key = calloc(1, sizeof(AES_KEY))) == NULL) {
		(void) fprintf(stderr, "aes256_init: alloc failure\n");
		free(crypt->encrypt_key);
		crypt->encrypt_key = NULL;
		return 0;
	}
	if (AES_set_decrypt_key(crypt->key, KEYBITS_AES256,
			crypt->decrypt_key)) {
		fprintf(stderr, "aes256_init: Error setting decrypt_key\n");
		free(crypt->encrypt_key);
		crypt->encrypt_key = NULL;
		free(crypt->decrypt_key);
		crypt->decrypt_key = NULL;
		return 0;
	}
	return 1;
}

static const pgp_crypt_t aes256 =
{
	PGP_SA_AES_256,
	AES_BLOCK_SIZE,
	KEYBITS_AES256 / 8,
	std_set_iv,
	std_set_key,
	aes256_init,
	std_resync,
	aes_block_encrypt,
	aes_block_decrypt,
	aes_cfb_encrypt,
	aes_cfb_decrypt,
	std_finish,
	TRAILER
};

/* Triple DES */

static int 
tripledes_init(pgp_crypt_t *crypt)
{
	DES_key_schedule *keys;
	int             n;

	if (crypt->encrypt_key) {
		free(crypt->encrypt_key);
	}
	if ((keys = crypt->encrypt_key = calloc(1, 3 * sizeof(DES_key_schedule))) == NULL) {
		(void) fprintf(stderr, "tripledes_init: alloc failure\n");
		return 0;
	}
	for (n = 0; n < 3; ++n) {
		DES_set_key((DES_cblock *)(void *)(crypt->key + n * 8),
			&keys[n]);
	}
	return 1;
}

static void 
tripledes_block_encrypt(pgp_crypt_t *crypt, void *out, const void *in)
{
	DES_key_schedule *keys = crypt->encrypt_key;

	DES_ecb3_encrypt(__UNCONST(in), out, &keys[0], &keys[1], &keys[2],
			DES_ENCRYPT);
}

static void 
tripledes_block_decrypt(pgp_crypt_t *crypt, void *out, const void *in)
{
	DES_key_schedule *keys = crypt->encrypt_key;

	DES_ecb3_encrypt(__UNCONST(in), out, &keys[0], &keys[1], &keys[2],
			DES_DECRYPT);
}

static void 
tripledes_cfb_encrypt(pgp_crypt_t *crypt, void *out, const void *in,
			size_t count)
{
	DES_key_schedule *keys = crypt->encrypt_key;

	DES_ede3_cfb64_encrypt(in, out, (long)count,
		&keys[0], &keys[1], &keys[2], (DES_cblock *)(void *)crypt->iv,
		&crypt->num, DES_ENCRYPT);
}

static void 
tripledes_cfb_decrypt(pgp_crypt_t *crypt, void *out, const void *in,
			size_t count)
{
	DES_key_schedule *keys = crypt->encrypt_key;

	DES_ede3_cfb64_encrypt(in, out, (long)count,
		&keys[0], &keys[1], &keys[2], (DES_cblock *)(void *)crypt->iv,
		&crypt->num, DES_DECRYPT);
}

static const pgp_crypt_t tripledes =
{
	PGP_SA_TRIPLEDES,
	8,
	24,
	std_set_iv,
	std_set_key,
	tripledes_init,
	std_resync,
	tripledes_block_encrypt,
	tripledes_block_decrypt,
	tripledes_cfb_encrypt,
	tripledes_cfb_decrypt,
	std_finish,
	TRAILER
};

#if defined(HAVE_OPENSSL_CAMELLIA_H) && !defined(OPENSSL_NO_CAMELLIA)
/* Camellia with 128-bit key (CAMELLIA) */

#define KEYBITS_CAMELLIA128 128

static int 
camellia128_init(pgp_crypt_t *crypt)
{
	if (crypt->encrypt_key) {
		free(crypt->encrypt_key);
	}
	if ((crypt->encrypt_key = calloc(1, sizeof(CAMELLIA_KEY))) == NULL) {
		(void) fprintf(stderr, "camellia128_init: alloc failure\n");
		return 0;
	}
	if (Camellia_set_key(crypt->key, KEYBITS_CAMELLIA128, crypt->encrypt_key)) {
		fprintf(stderr, "camellia128_init: Error setting encrypt_key\n");
	}
	if (crypt->decrypt_key) {
		free(crypt->decrypt_key);
	}
	if ((crypt->decrypt_key = calloc(1, sizeof(CAMELLIA_KEY))) == NULL) {
		(void) fprintf(stderr, "camellia128_init: alloc failure\n");
		return 0;
	}
	if (Camellia_set_key(crypt->key, KEYBITS_CAMELLIA128, crypt->decrypt_key)) {
		fprintf(stderr, "camellia128_init: Error setting decrypt_key\n");
	}
	return 1;
}

static void 
camellia_block_encrypt(pgp_crypt_t *crypt, void *out, const void *in)
{
	Camellia_encrypt(in, out, crypt->encrypt_key);
}

static void 
camellia_block_decrypt(pgp_crypt_t *crypt, void *out, const void *in)
{
	Camellia_decrypt(in, out, crypt->decrypt_key);
}

static void 
camellia_cfb_encrypt(pgp_crypt_t *crypt, void *out, const void *in, size_t count)
{
	Camellia_cfb128_encrypt(in, out, (unsigned)count,
			   crypt->encrypt_key, crypt->iv, &crypt->num,
			   CAMELLIA_ENCRYPT);
}

static void 
camellia_cfb_decrypt(pgp_crypt_t *crypt, void *out, const void *in, size_t count)
{
	Camellia_cfb128_encrypt(in, out, (unsigned)count,
			   crypt->encrypt_key, crypt->iv, &crypt->num,
			   CAMELLIA_DECRYPT);
}

static const pgp_crypt_t camellia128 =
{
	PGP_SA_CAMELLIA_128,
	CAMELLIA_BLOCK_SIZE,
	KEYBITS_CAMELLIA128 / 8,
	std_set_iv,
	std_set_key,
	camellia128_init,
	std_resync,
	camellia_block_encrypt,
	camellia_block_decrypt,
	camellia_cfb_encrypt,
	camellia_cfb_decrypt,
	std_finish,
	TRAILER
};

/* Camellia with 256-bit key (CAMELLIA) */

#define KEYBITS_CAMELLIA256 256

static int 
camellia256_init(pgp_crypt_t *crypt)
{
	if (crypt->encrypt_key) {
		free(crypt->encrypt_key);
	}
	if ((crypt->encrypt_key = calloc(1, sizeof(CAMELLIA_KEY))) == NULL) {
		(void) fprintf(stderr, "camellia256_init: alloc failure\n");
		return 0;
	}
	if (Camellia_set_key(crypt->key, KEYBITS_CAMELLIA256, crypt->encrypt_key)) {
		fprintf(stderr, "camellia256_init: Error setting encrypt_key\n");
	}
	if (crypt->decrypt_key) {
		free(crypt->decrypt_key);
	}
	if ((crypt->decrypt_key = calloc(1, sizeof(CAMELLIA_KEY))) == NULL) {
		(void) fprintf(stderr, "camellia256_init: alloc failure\n");
		return 0;
	}
	if (Camellia_set_key(crypt->key, KEYBITS_CAMELLIA256, crypt->decrypt_key)) {
		fprintf(stderr, "camellia256_init: Error setting decrypt_key\n");
	}
	return 1;
}

static const pgp_crypt_t camellia256 =
{
	PGP_SA_CAMELLIA_256,
	CAMELLIA_BLOCK_SIZE,
	KEYBITS_CAMELLIA256 / 8,
	std_set_iv,
	std_set_key,
	camellia256_init,
	std_resync,
	camellia_block_encrypt,
	camellia_block_decrypt,
	camellia_cfb_encrypt,
	camellia_cfb_decrypt,
	std_finish,
	TRAILER
};
#endif


static const pgp_crypt_t *
get_proto(pgp_symm_alg_t alg)
{
	switch (alg) {
	case PGP_SA_CAST5:
		return &cast5;
#ifndef OPENSSL_NO_IDEA
	case PGP_SA_IDEA:
		return &idea;
#endif				/* OPENSSL_NO_IDEA */
	case PGP_SA_AES_128:
		return &aes128;
	case PGP_SA_AES_256:
		return &aes256;
#if defined(HAVE_OPENSSL_CAMELLIA_H) && !defined(OPENSSL_NO_CAMELLIA)
	case PGP_SA_CAMELLIA_128:
		return &camellia128;
	case PGP_SA_CAMELLIA_256:
		return &camellia256;
#endif
	case PGP_SA_TRIPLEDES:
		return &tripledes;
	default:
		(void) fprintf(stderr, "Unknown algorithm: %d (%s)\n",
			alg, pgp_show_symm_alg(alg));
	}
	return NULL;
}

int 
pgp_crypt_any(pgp_crypt_t *crypt, pgp_symm_alg_t alg)
{
	const pgp_crypt_t *ptr = get_proto(alg);

	if (ptr) {
		*crypt = *ptr;
		return 1;
	} else {
		(void) memset(crypt, 0x0, sizeof(*crypt));
		return 0;
	}
}

unsigned 
pgp_block_size(pgp_symm_alg_t alg)
{
	const pgp_crypt_t *p = get_proto(alg);

	return (p == NULL) ? 0 : (unsigned)p->blocksize;
}

unsigned 
pgp_key_size(pgp_symm_alg_t alg)
{
	const pgp_crypt_t *p = get_proto(alg);

	return (p == NULL) ? 0 : (unsigned)p->keysize;
}

void 
pgp_encrypt_init(pgp_crypt_t *encrypt)
{
	/* \todo should there be a separate pgp_encrypt_init? */
	pgp_decrypt_init(encrypt);
}

void 
pgp_decrypt_init(pgp_crypt_t *decrypt)
{
	decrypt->base_init(decrypt);
	decrypt->block_encrypt(decrypt, decrypt->siv, decrypt->iv);
	(void) memcpy(decrypt->civ, decrypt->siv, decrypt->blocksize);
	decrypt->num = 0;
}

size_t
pgp_decrypt_se(pgp_crypt_t *decrypt, void *outvoid, const void *invoid,
		size_t count)
{
	const uint8_t	*in = invoid;
	uint8_t		*out = outvoid;
	int              saved = (int)count;

	/*
	 * in order to support v3's weird resyncing we have to implement CFB
	 * mode ourselves
	 */
	while (count-- > 0) {
		uint8_t   t;

		if ((size_t) decrypt->num == decrypt->blocksize) {
			(void) memcpy(decrypt->siv, decrypt->civ,
					decrypt->blocksize);
			decrypt->block_decrypt(decrypt, decrypt->civ,
					decrypt->civ);
			decrypt->num = 0;
		}
		t = decrypt->civ[decrypt->num];
		*out++ = t ^ (decrypt->civ[decrypt->num++] = *in++);
	}

	return (size_t)saved;
}

size_t 
pgp_encrypt_se(pgp_crypt_t *encrypt, void *outvoid, const void *invoid,
	       size_t count)
{
	const uint8_t	*in = invoid;
	uint8_t		*out = outvoid;
	int              saved = (int)count;

	/*
	 * in order to support v3's weird resyncing we have to implement CFB
	 * mode ourselves
	 */
	while (count-- > 0) {
		if ((size_t) encrypt->num == encrypt->blocksize) {
			(void) memcpy(encrypt->siv, encrypt->civ,
					encrypt->blocksize);
			encrypt->block_encrypt(encrypt, encrypt->civ,
					encrypt->civ);
			encrypt->num = 0;
		}
		encrypt->civ[encrypt->num] = *out++ =
				encrypt->civ[encrypt->num] ^ *in++;
		++encrypt->num;
	}

	return (size_t)saved;
}

/**
\ingroup HighLevel_Supported
\brief Is this Symmetric Algorithm supported?
\param alg Symmetric Algorithm to check
\return 1 if supported; else 0
*/
unsigned 
pgp_is_sa_supported(pgp_symm_alg_t alg)
{
	switch (alg) {
	case PGP_SA_AES_128:
	case PGP_SA_AES_256:
	case PGP_SA_CAST5:
	case PGP_SA_TRIPLEDES:
#if defined(HAVE_OPENSSL_CAMELLIA_H) && !defined(OPENSSL_NO_CAMELLIA)
	case PGP_SA_CAMELLIA_128:
	case PGP_SA_CAMELLIA_256:
#endif
#ifndef OPENSSL_NO_IDEA
	case PGP_SA_IDEA:
#endif
		return 1;

	default:
		fprintf(stderr, "\nWarning: %s not supported\n",
			pgp_show_symm_alg(alg));
		return 0;
	}
}

size_t 
pgp_encrypt_se_ip(pgp_crypt_t *crypt, void *out, const void *in,
		  size_t count)
{
	if (!pgp_is_sa_supported(crypt->alg)) {
		return 0;
	}

	crypt->cfb_encrypt(crypt, out, in, count);

	/* \todo test this number was encrypted */
	return count;
}

size_t 
pgp_decrypt_se_ip(pgp_crypt_t *crypt, void *out, const void *in,
		  size_t count)
{
	if (!pgp_is_sa_supported(crypt->alg)) {
		return 0;
	}

	crypt->cfb_decrypt(crypt, out, in, count);

	/* \todo check this number was in fact decrypted */
	return count;
}
