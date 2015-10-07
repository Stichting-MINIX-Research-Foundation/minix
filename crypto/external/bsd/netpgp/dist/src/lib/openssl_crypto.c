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

/** \file
 */
#include "config.h"

#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif

#if defined(__NetBSD__)
__COPYRIGHT("@(#) Copyright (c) 2009 The NetBSD Foundation, Inc. All rights reserved.");
__RCSID("$NetBSD: openssl_crypto.c,v 1.33 2010/11/07 08:39:59 agc Exp $");
#endif

#ifdef HAVE_OPENSSL_DSA_H
#include <openssl/dsa.h>
#endif

#ifdef HAVE_OPENSSL_RSA_H
#include <openssl/rsa.h>
#endif

#ifdef HAVE_OPENSSL_ERR_H
#include <openssl/err.h>
#endif

#include <openssl/pem.h>
#include <openssl/evp.h>

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "crypto.h"
#include "keyring.h"
#include "readerwriter.h"
#include "netpgpdefs.h"
#include "netpgpdigest.h"
#include "packet.h"


static void 
test_seckey(const pgp_seckey_t *seckey)
{
	RSA            *test = RSA_new();

	test->n = BN_dup(seckey->pubkey.key.rsa.n);
	test->e = BN_dup(seckey->pubkey.key.rsa.e);

	test->d = BN_dup(seckey->key.rsa.d);
	test->p = BN_dup(seckey->key.rsa.p);
	test->q = BN_dup(seckey->key.rsa.q);

	if (RSA_check_key(test) != 1) {
		(void) fprintf(stderr,
			"test_seckey: RSA_check_key failed\n");
	}
	RSA_free(test);
}

static int 
md5_init(pgp_hash_t *hash)
{
	if (hash->data) {
		(void) fprintf(stderr, "md5_init: hash data non-null\n");
	}
	if ((hash->data = calloc(1, sizeof(MD5_CTX))) == NULL) {
		(void) fprintf(stderr, "md5_init: bad alloc\n");
		return 0;
	}
	MD5_Init(hash->data);
	return 1;
}

static void 
md5_add(pgp_hash_t *hash, const uint8_t *data, unsigned length)
{
	MD5_Update(hash->data, data, length);
}

static unsigned 
md5_finish(pgp_hash_t *hash, uint8_t *out)
{
	MD5_Final(out, hash->data);
	free(hash->data);
	hash->data = NULL;
	return 16;
}

static const pgp_hash_t md5 = {
	PGP_HASH_MD5,
	MD5_DIGEST_LENGTH,
	"MD5",
	md5_init,
	md5_add,
	md5_finish,
	NULL
};

/**
   \ingroup Core_Crypto
   \brief Initialise to MD5
   \param hash Hash to initialise
*/
void 
pgp_hash_md5(pgp_hash_t *hash)
{
	*hash = md5;
}

static int 
sha1_init(pgp_hash_t *hash)
{
	if (hash->data) {
		(void) fprintf(stderr, "sha1_init: hash data non-null\n");
	}
	if ((hash->data = calloc(1, sizeof(SHA_CTX))) == NULL) {
		(void) fprintf(stderr, "sha1_init: bad alloc\n");
		return 0;
	}
	SHA1_Init(hash->data);
	return 1;
}

static void 
sha1_add(pgp_hash_t *hash, const uint8_t *data, unsigned length)
{
	if (pgp_get_debug_level(__FILE__)) {
		hexdump(stderr, "sha1_add", data, length);
	}
	SHA1_Update(hash->data, data, length);
}

static unsigned 
sha1_finish(pgp_hash_t *hash, uint8_t *out)
{
	SHA1_Final(out, hash->data);
	if (pgp_get_debug_level(__FILE__)) {
		hexdump(stderr, "sha1_finish", out, PGP_SHA1_HASH_SIZE);
	}
	free(hash->data);
	hash->data = NULL;
	return PGP_SHA1_HASH_SIZE;
}

static const pgp_hash_t sha1 = {
	PGP_HASH_SHA1,
	PGP_SHA1_HASH_SIZE,
	"SHA1",
	sha1_init,
	sha1_add,
	sha1_finish,
	NULL
};

/**
   \ingroup Core_Crypto
   \brief Initialise to SHA1
   \param hash Hash to initialise
*/
void 
pgp_hash_sha1(pgp_hash_t *hash)
{
	*hash = sha1;
}

static int 
sha256_init(pgp_hash_t *hash)
{
	if (hash->data) {
		(void) fprintf(stderr, "sha256_init: hash data non-null\n");
	}
	if ((hash->data = calloc(1, sizeof(SHA256_CTX))) == NULL) {
		(void) fprintf(stderr, "sha256_init: bad alloc\n");
		return 0;
	}
	SHA256_Init(hash->data);
	return 1;
}

static void 
sha256_add(pgp_hash_t *hash, const uint8_t *data, unsigned length)
{
	if (pgp_get_debug_level(__FILE__)) {
		hexdump(stderr, "sha256_add", data, length);
	}
	SHA256_Update(hash->data, data, length);
}

static unsigned 
sha256_finish(pgp_hash_t *hash, uint8_t *out)
{
	SHA256_Final(out, hash->data);
	if (pgp_get_debug_level(__FILE__)) {
		hexdump(stderr, "sha1_finish", out, SHA256_DIGEST_LENGTH);
	}
	free(hash->data);
	hash->data = NULL;
	return SHA256_DIGEST_LENGTH;
}

static const pgp_hash_t sha256 = {
	PGP_HASH_SHA256,
	SHA256_DIGEST_LENGTH,
	"SHA256",
	sha256_init,
	sha256_add,
	sha256_finish,
	NULL
};

void 
pgp_hash_sha256(pgp_hash_t *hash)
{
	*hash = sha256;
}

/*
 * SHA384
 */
static int 
sha384_init(pgp_hash_t *hash)
{
	if (hash->data) {
		(void) fprintf(stderr, "sha384_init: hash data non-null\n");
	}
	if ((hash->data = calloc(1, sizeof(SHA512_CTX))) == NULL) {
		(void) fprintf(stderr, "sha384_init: bad alloc\n");
		return 0;
	}
	SHA384_Init(hash->data);
	return 1;
}

static void 
sha384_add(pgp_hash_t *hash, const uint8_t *data, unsigned length)
{
	if (pgp_get_debug_level(__FILE__)) {
		hexdump(stderr, "sha384_add", data, length);
	}
	SHA384_Update(hash->data, data, length);
}

static unsigned 
sha384_finish(pgp_hash_t *hash, uint8_t *out)
{
	SHA384_Final(out, hash->data);
	if (pgp_get_debug_level(__FILE__)) {
		hexdump(stderr, "sha384_finish", out, SHA384_DIGEST_LENGTH);
	}
	free(hash->data);
	hash->data = NULL;
	return SHA384_DIGEST_LENGTH;
}

static const pgp_hash_t sha384 = {
	PGP_HASH_SHA384,
	SHA384_DIGEST_LENGTH,
	"SHA384",
	sha384_init,
	sha384_add,
	sha384_finish,
	NULL
};

void 
pgp_hash_sha384(pgp_hash_t *hash)
{
	*hash = sha384;
}

/*
 * SHA512
 */
static int 
sha512_init(pgp_hash_t *hash)
{
	if (hash->data) {
		(void) fprintf(stderr, "sha512_init: hash data non-null\n");
	}
	if ((hash->data = calloc(1, sizeof(SHA512_CTX))) == NULL) {
		(void) fprintf(stderr, "sha512_init: bad alloc\n");
		return 0;
	}
	SHA512_Init(hash->data);
	return 1;
}

static void 
sha512_add(pgp_hash_t *hash, const uint8_t *data, unsigned length)
{
	if (pgp_get_debug_level(__FILE__)) {
		hexdump(stderr, "sha512_add", data, length);
	}
	SHA512_Update(hash->data, data, length);
}

static unsigned 
sha512_finish(pgp_hash_t *hash, uint8_t *out)
{
	SHA512_Final(out, hash->data);
	if (pgp_get_debug_level(__FILE__)) {
		hexdump(stderr, "sha512_finish", out, SHA512_DIGEST_LENGTH);
	}
	free(hash->data);
	hash->data = NULL;
	return SHA512_DIGEST_LENGTH;
}

static const pgp_hash_t sha512 = {
	PGP_HASH_SHA512,
	SHA512_DIGEST_LENGTH,
	"SHA512",
	sha512_init,
	sha512_add,
	sha512_finish,
	NULL
};

void 
pgp_hash_sha512(pgp_hash_t *hash)
{
	*hash = sha512;
}

/*
 * SHA224
 */

static int 
sha224_init(pgp_hash_t *hash)
{
	if (hash->data) {
		(void) fprintf(stderr, "sha224_init: hash data non-null\n");
	}
	if ((hash->data = calloc(1, sizeof(SHA256_CTX))) == NULL) {
		(void) fprintf(stderr, "sha256_init: bad alloc\n");
		return 0;
	}
	SHA224_Init(hash->data);
	return 1;
}

static void 
sha224_add(pgp_hash_t *hash, const uint8_t *data, unsigned length)
{
	if (pgp_get_debug_level(__FILE__)) {
		hexdump(stderr, "sha224_add", data, length);
	}
	SHA224_Update(hash->data, data, length);
}

static unsigned 
sha224_finish(pgp_hash_t *hash, uint8_t *out)
{
	SHA224_Final(out, hash->data);
	if (pgp_get_debug_level(__FILE__)) {
		hexdump(stderr, "sha224_finish", out, SHA224_DIGEST_LENGTH);
	}
	free(hash->data);
	hash->data = NULL;
	return SHA224_DIGEST_LENGTH;
}

static const pgp_hash_t sha224 = {
	PGP_HASH_SHA224,
	SHA224_DIGEST_LENGTH,
	"SHA224",
	sha224_init,
	sha224_add,
	sha224_finish,
	NULL
};

void 
pgp_hash_sha224(pgp_hash_t *hash)
{
	*hash = sha224;
}

unsigned 
pgp_dsa_verify(const uint8_t *hash, size_t hash_length,
	       const pgp_dsa_sig_t *sig,
	       const pgp_dsa_pubkey_t *dsa)
{
	unsigned	qlen;
	DSA_SIG        *osig;
	DSA            *odsa;
	int             ret;

	osig = DSA_SIG_new();
	osig->r = sig->r;
	osig->s = sig->s;

	odsa = DSA_new();
	odsa->p = dsa->p;
	odsa->q = dsa->q;
	odsa->g = dsa->g;
	odsa->pub_key = dsa->y;

	if (pgp_get_debug_level(__FILE__)) {
		hexdump(stderr, "input hash", hash, hash_length);
		(void) fprintf(stderr, "Q=%d\n", BN_num_bytes(odsa->q));
	}
	if ((qlen = (unsigned)BN_num_bytes(odsa->q)) < hash_length) {
		hash_length = qlen;
	}
	ret = DSA_do_verify(hash, (int)hash_length, osig, odsa);
	if (pgp_get_debug_level(__FILE__)) {
		(void) fprintf(stderr, "ret=%d\n", ret);
	}
	if (ret < 0) {
		(void) fprintf(stderr, "pgp_dsa_verify: DSA verification\n");
		return 0;
	}

	odsa->p = odsa->q = odsa->g = odsa->pub_key = NULL;
	DSA_free(odsa);

	osig->r = osig->s = NULL;
	DSA_SIG_free(osig);

	return (unsigned)ret;
}

/**
   \ingroup Core_Crypto
   \brief Recovers message digest from the signature
   \param out Where to write decrypted data to
   \param in Encrypted data
   \param length Length of encrypted data
   \param pubkey RSA public key
   \return size of recovered message digest
*/
int 
pgp_rsa_public_decrypt(uint8_t *out,
			const uint8_t *in,
			size_t length,
			const pgp_rsa_pubkey_t *pubkey)
{
	RSA            *orsa;
	int             n;

	orsa = RSA_new();
	orsa->n = pubkey->n;
	orsa->e = pubkey->e;

	n = RSA_public_decrypt((int)length, in, out, orsa, RSA_NO_PADDING);

	orsa->n = orsa->e = NULL;
	RSA_free(orsa);

	return n;
}

/**
   \ingroup Core_Crypto
   \brief Signs data with RSA
   \param out Where to write signature
   \param in Data to sign
   \param length Length of data
   \param seckey RSA secret key
   \param pubkey RSA public key
   \return number of bytes decrypted
*/
int 
pgp_rsa_private_encrypt(uint8_t *out,
			const uint8_t *in,
			size_t length,
			const pgp_rsa_seckey_t *seckey,
			const pgp_rsa_pubkey_t *pubkey)
{
	RSA            *orsa;
	int             n;

	orsa = RSA_new();
	orsa->n = BN_dup(pubkey->n);
	orsa->d = seckey->d;
	orsa->p = seckey->q;	/* p and q are round the other way in openssl */
	orsa->q = seckey->p;

	/* debug */
	orsa->e = BN_dup(pubkey->e);
	/* If this isn't set, it's very likely that the programmer hasn't */
	/* decrypted the secret key. RSA_check_key segfaults in that case. */
	/* Use pgp_decrypt_seckey() to do that. */
	if (orsa->d == NULL) {
		(void) fprintf(stderr, "orsa is not set\n");
		return 0;
	}
	if (RSA_check_key(orsa) != 1) {
		(void) fprintf(stderr, "RSA_check_key is not set\n");
		return 0;
	}
	/* end debug */

	n = RSA_private_encrypt((int)length, in, out, orsa, RSA_NO_PADDING);

	orsa->n = orsa->d = orsa->p = orsa->q = NULL;
	RSA_free(orsa);

	return n;
}

/**
\ingroup Core_Crypto
\brief Decrypts RSA-encrypted data
\param out Where to write the plaintext
\param in Encrypted data
\param length Length of encrypted data
\param seckey RSA secret key
\param pubkey RSA public key
\return size of recovered plaintext
*/
int 
pgp_rsa_private_decrypt(uint8_t *out,
			const uint8_t *in,
			size_t length,
			const pgp_rsa_seckey_t *seckey,
			const pgp_rsa_pubkey_t *pubkey)
{
	RSA            *keypair;
	int             n;
	char            errbuf[1024];

	keypair = RSA_new();
	keypair->n = pubkey->n;	/* XXX: do we need n? */
	keypair->d = seckey->d;
	keypair->p = seckey->q;
	keypair->q = seckey->p;

	/* debug */
	keypair->e = pubkey->e;
	if (RSA_check_key(keypair) != 1) {
		(void) fprintf(stderr, "RSA_check_key is not set\n");
		return 0;
	}
	/* end debug */

	n = RSA_private_decrypt((int)length, in, out, keypair, RSA_NO_PADDING);

	if (pgp_get_debug_level(__FILE__)) {
		printf("pgp_rsa_private_decrypt: n=%d\n",n);
	}

	errbuf[0] = '\0';
	if (n == -1) {
		unsigned long   err = ERR_get_error();

		ERR_error_string(err, &errbuf[0]);
		(void) fprintf(stderr, "openssl error : %s\n", errbuf);
	}
	keypair->n = keypair->d = keypair->p = keypair->q = NULL;
	RSA_free(keypair);

	return n;
}

/**
   \ingroup Core_Crypto
   \brief RSA-encrypts data
   \param out Where to write the encrypted data
   \param in Plaintext
   \param length Size of plaintext
   \param pubkey RSA Public Key
*/
int 
pgp_rsa_public_encrypt(uint8_t *out,
			const uint8_t *in,
			size_t length,
			const pgp_rsa_pubkey_t *pubkey)
{
	RSA            *orsa;
	int             n;

	/* printf("pgp_rsa_public_encrypt: length=%ld\n", length); */

	orsa = RSA_new();
	orsa->n = pubkey->n;
	orsa->e = pubkey->e;

	/* printf("len: %ld\n", length); */
	/* pgp_print_bn("n: ", orsa->n); */
	/* pgp_print_bn("e: ", orsa->e); */
	n = RSA_public_encrypt((int)length, in, out, orsa, RSA_NO_PADDING);

	if (n == -1) {
		BIO            *fd_out;

		fd_out = BIO_new_fd(fileno(stderr), BIO_NOCLOSE);
		ERR_print_errors(fd_out);
	}
	orsa->n = orsa->e = NULL;
	RSA_free(orsa);

	return n;
}

/**
   \ingroup Core_Crypto
   \brief Finalise openssl
   \note Would usually call pgp_finish() instead
   \sa pgp_finish()
*/
void 
pgp_crypto_finish(void)
{
	CRYPTO_cleanup_all_ex_data();
	ERR_remove_state((unsigned long)0);
}

/**
   \ingroup Core_Hashes
   \brief Get Hash name
   \param hash Hash struct
   \return Hash name
*/
const char     *
pgp_text_from_hash(pgp_hash_t *hash)
{
	return hash->name;
}

/**
 \ingroup HighLevel_KeyGenerate
 \brief Generates an RSA keypair
 \param numbits Modulus size
 \param e Public Exponent
 \param keydata Pointer to keydata struct to hold new key
 \return 1 if key generated successfully; otherwise 0
 \note It is the caller's responsibility to call pgp_keydata_free(keydata)
*/
static unsigned 
rsa_generate_keypair(pgp_key_t *keydata,
			const int numbits,
			const unsigned long e,
			const char *hashalg,
			const char *cipher)
{
	pgp_seckey_t *seckey;
	RSA            *rsa;
	BN_CTX         *ctx;
	pgp_output_t *output;
	pgp_memory_t   *mem;

	ctx = BN_CTX_new();
	pgp_keydata_init(keydata, PGP_PTAG_CT_SECRET_KEY);
	seckey = pgp_get_writable_seckey(keydata);

	/* generate the key pair */

	rsa = RSA_generate_key(numbits, e, NULL, NULL);

	/* populate pgp key from ssl key */

	seckey->pubkey.version = PGP_V4;
	seckey->pubkey.birthtime = time(NULL);
	seckey->pubkey.days_valid = 0;
	seckey->pubkey.alg = PGP_PKA_RSA;

	seckey->pubkey.key.rsa.n = BN_dup(rsa->n);
	seckey->pubkey.key.rsa.e = BN_dup(rsa->e);

	seckey->s2k_usage = PGP_S2KU_ENCRYPTED_AND_HASHED;
	seckey->s2k_specifier = PGP_S2KS_SALTED;
	/* seckey->s2k_specifier=PGP_S2KS_SIMPLE; */
	if ((seckey->hash_alg = pgp_str_to_hash_alg(hashalg)) == PGP_HASH_UNKNOWN) {
		seckey->hash_alg = PGP_HASH_SHA1;
	}
	seckey->alg = pgp_str_to_cipher(cipher);
	seckey->octetc = 0;
	seckey->checksum = 0;

	seckey->key.rsa.d = BN_dup(rsa->d);
	seckey->key.rsa.p = BN_dup(rsa->p);
	seckey->key.rsa.q = BN_dup(rsa->q);
	seckey->key.rsa.u = BN_mod_inverse(NULL, rsa->p, rsa->q, ctx);
	if (seckey->key.rsa.u == NULL) {
		(void) fprintf(stderr, "seckey->key.rsa.u is NULL\n");
		return 0;
	}
	BN_CTX_free(ctx);

	RSA_free(rsa);

	pgp_keyid(keydata->sigid, PGP_KEY_ID_SIZE, &keydata->key.seckey.pubkey, seckey->hash_alg);
	pgp_fingerprint(&keydata->sigfingerprint, &keydata->key.seckey.pubkey, seckey->hash_alg);

	/* Generate checksum */

	output = NULL;
	mem = NULL;

	pgp_setup_memory_write(&output, &mem, 128);

	pgp_push_checksum_writer(output, seckey);

	switch (seckey->pubkey.alg) {
	case PGP_PKA_DSA:
		return pgp_write_mpi(output, seckey->key.dsa.x);
	case PGP_PKA_RSA:
	case PGP_PKA_RSA_ENCRYPT_ONLY:
	case PGP_PKA_RSA_SIGN_ONLY:
		if (!pgp_write_mpi(output, seckey->key.rsa.d) ||
		    !pgp_write_mpi(output, seckey->key.rsa.p) ||
		    !pgp_write_mpi(output, seckey->key.rsa.q) ||
		    !pgp_write_mpi(output, seckey->key.rsa.u)) {
			return 0;
		}
		break;
	case PGP_PKA_ELGAMAL:
		return pgp_write_mpi(output, seckey->key.elgamal.x);

	default:
		(void) fprintf(stderr, "Bad seckey->pubkey.alg\n");
		return 0;
	}

	/* close rather than pop, since its the only one on the stack */
	pgp_writer_close(output);
	pgp_teardown_memory_write(output, mem);

	/* should now have checksum in seckey struct */

	/* test */
	if (pgp_get_debug_level(__FILE__)) {
		test_seckey(seckey);
	}

	return 1;
}

/**
 \ingroup HighLevel_KeyGenerate
 \brief Creates a self-signed RSA keypair
 \param numbits Modulus size
 \param e Public Exponent
 \param userid User ID
 \return The new keypair or NULL

 \note It is the caller's responsibility to call pgp_keydata_free(keydata)
 \sa rsa_generate_keypair()
 \sa pgp_keydata_free()
*/
pgp_key_t  *
pgp_rsa_new_selfsign_key(const int numbits,
				const unsigned long e,
				uint8_t *userid,
				const char *hashalg,
				const char *cipher)
{
	pgp_key_t  *keydata;

	keydata = pgp_keydata_new();
	if (!rsa_generate_keypair(keydata, numbits, e, hashalg, cipher) ||
	    !pgp_add_selfsigned_userid(keydata, userid)) {
		pgp_keydata_free(keydata);
		return NULL;
	}
	return keydata;
}

DSA_SIG        *
pgp_dsa_sign(uint8_t *hashbuf,
		unsigned hashsize,
		const pgp_dsa_seckey_t *secdsa,
		const pgp_dsa_pubkey_t *pubdsa)
{
	DSA_SIG        *dsasig;
	DSA            *odsa;

	odsa = DSA_new();
	odsa->p = pubdsa->p;
	odsa->q = pubdsa->q;
	odsa->g = pubdsa->g;
	odsa->pub_key = pubdsa->y;
	odsa->priv_key = secdsa->x;

	dsasig = DSA_do_sign(hashbuf, (int)hashsize, odsa);

	odsa->p = odsa->q = odsa->g = odsa->pub_key = odsa->priv_key = NULL;
	DSA_free(odsa);

	return dsasig;
}

int
openssl_read_pem_seckey(const char *f, pgp_key_t *key, const char *type, int verbose)
{
	FILE	*fp;
	char	 prompt[BUFSIZ];
	char	*pass;
	DSA	*dsa;
	RSA	*rsa;
	int	 ok;

	OpenSSL_add_all_algorithms();
	if ((fp = fopen(f, "r")) == NULL) {
		if (verbose) {
			(void) fprintf(stderr, "can't open '%s'\n", f);
		}
		return 0;
	}
	ok = 1;
	if (strcmp(type, "ssh-rsa") == 0) {
		if ((rsa = PEM_read_RSAPrivateKey(fp, NULL, NULL, NULL)) == NULL) {
			(void) snprintf(prompt, sizeof(prompt), "netpgp PEM %s passphrase: ", f);
			do {
				pass = getpass(prompt);
				rsa = PEM_read_RSAPrivateKey(fp, NULL, NULL, pass);
			} while (rsa == NULL);
		}
		key->key.seckey.key.rsa.d = rsa->d;
		key->key.seckey.key.rsa.p = rsa->p;
		key->key.seckey.key.rsa.q = rsa->q;
		key->key.seckey.key.rsa.d = rsa->d;
	} else if (strcmp(type, "ssh-dss") == 0) {
		if ((dsa = PEM_read_DSAPrivateKey(fp, NULL, NULL, NULL)) == NULL) {
			ok = 0;
		} else {
			key->key.seckey.key.dsa.x = dsa->priv_key;
		}
	} else {
		ok = 0;
	}
	(void) fclose(fp);
	return ok;
}

/*
 * Decide the number of bits in the random componont k
 *
 * It should be in the same range as p for signing (which
 * is deprecated), but can be much smaller for encrypting.
 *
 * Until I research it further, I just mimic gpg behaviour.
 * It has a special mapping table, for values <= 5120,
 * above that it uses 'arbitrary high number'.	Following
 * algorihm hovers 10-70 bits above gpg values.  And for
 * larger p, it uses gpg's algorihm.
 *
 * The point is - if k gets large, encryption will be
 * really slow.  It does not matter for decryption.
 */
static int
decide_k_bits(int p_bits)
{
	return (p_bits <= 5120) ? p_bits / 10 + 160 : (p_bits / 8 + 200) * 3 / 2;
}

int
pgp_elgamal_public_encrypt(uint8_t *g_to_k, uint8_t *encm,
			const uint8_t *in,
			size_t size,
			const pgp_elgamal_pubkey_t *pubkey)
{
	int	ret = 0;
	int	k_bits;
	BIGNUM	   *m;
	BIGNUM	   *p;
	BIGNUM	   *g;
	BIGNUM	   *y;
	BIGNUM	   *k;
	BIGNUM	   *yk;
	BIGNUM	   *c1;
	BIGNUM	   *c2;
	BN_CTX	   *tmp;

	m = BN_bin2bn(in, (int)size, NULL);
	p = pubkey->p;
	g = pubkey->g;
	y = pubkey->y;
	k = BN_new();
	yk = BN_new();
	c1 = BN_new();
	c2 = BN_new();
	tmp = BN_CTX_new();
	if (!m || !p || !g || !y || !k || !yk || !c1 || !c2 || !tmp) {
		goto done;
	}
	/*
	 * generate k
	 */
	k_bits = decide_k_bits(BN_num_bits(p));
	if (!BN_rand(k, k_bits, 0, 0)) {
		goto done;
	}
	/*
	 * c1 = g^k c2 = m * y^k
	 */
	if (!BN_mod_exp(c1, g, k, p, tmp)) {
		goto done;
	}
	if (!BN_mod_exp(yk, y, k, p, tmp)) {
		goto done;
	}
	if (!BN_mod_mul(c2, m, yk, p, tmp)) {
		goto done;
	}
	/* result */
	BN_bn2bin(c1, g_to_k);
	ret = BN_num_bytes(c1);	/* c1 = g^k */
	BN_bn2bin(c2, encm);
	ret += BN_num_bytes(c2); /* c2 = m * y^k */
done:
	if (tmp) {
		BN_CTX_free(tmp);
	}
	if (c2) {
		BN_clear_free(c2);
	}
	if (c1) {
		BN_clear_free(c1);
	}
	if (yk) {
		BN_clear_free(yk);
	}
	if (k) {
		BN_clear_free(k);
	}
	if (g) {
		BN_clear_free(g);
	}
	return ret;
}

int
pgp_elgamal_private_decrypt(uint8_t *out,
				const uint8_t *g_to_k,
				const uint8_t *in,
				size_t length,
				const pgp_elgamal_seckey_t *seckey,
				const pgp_elgamal_pubkey_t *pubkey)
{
	BIGNUM	*bndiv;
	BIGNUM	*c1x;
	BN_CTX	*tmp;
	BIGNUM	*c1;
	BIGNUM	*c2;
	BIGNUM	*p;
	BIGNUM	*x;
	BIGNUM	*m;
	int	 ret;

	ret = 0;
	/* c1 and c2 are in g_to_k and in, respectively*/
	c1 = BN_bin2bn(g_to_k, (int)length, NULL);
	c2 = BN_bin2bn(in, (int)length, NULL);
	/* other bits */
	p = pubkey->p;
	x = seckey->x;
	c1x = BN_new();
	bndiv = BN_new();
	m = BN_new();
	tmp = BN_CTX_new();
	if (!c1 || !c2 || !p || !x || !c1x || !bndiv || !m || !tmp) {
		goto done;
	}
	/*
	 * m = c2 / (c1^x)
	 */
	if (!BN_mod_exp(c1x, c1, x, p, tmp)) {
		goto done;
	}
	if (!BN_mod_inverse(bndiv, c1x, p, tmp)) {
		goto done;
	}
	if (!BN_mod_mul(m, c2, bndiv, p, tmp)) {
		goto done;
	}
	/* result */
	ret = BN_bn2bin(m, out);
done:
	if (tmp) {
		BN_CTX_free(tmp);
	}
	if (m) {
		BN_clear_free(m);
	}
	if (bndiv) {
		BN_clear_free(bndiv);
	}
	if (c1x) {
		BN_clear_free(c1x);
	}
	if (x) {
		BN_clear_free(x);
	}
	if (p) {
		BN_clear_free(p);
	}
	if (c1) {
		BN_clear_free(c1);
	}
	if (c2) {
		BN_clear_free(c2);
	}
	return ret;
}
