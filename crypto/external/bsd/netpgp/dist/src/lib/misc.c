/*-
 * Copyright (c) 2009,2010 The NetBSD Foundation, Inc.
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
__RCSID("$NetBSD: misc.c,v 1.41 2012/03/05 02:20:18 christos Exp $");
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_OPENSSL_RAND_H
#include <openssl/rand.h>
#endif

#include "errors.h"
#include "packet.h"
#include "crypto.h"
#include "create.h"
#include "packet-parse.h"
#include "packet-show.h"
#include "signature.h"
#include "netpgpsdk.h"
#include "netpgpdefs.h"
#include "memory.h"
#include "readerwriter.h"
#include "version.h"
#include "netpgpdigest.h"

#ifdef WIN32
#define vsnprintf _vsnprintf
#endif


typedef struct {
	pgp_keyring_t		*keyring;
} accumulate_t;

/**
 * \ingroup Core_Callbacks
 */
static pgp_cb_ret_t
accumulate_cb(const pgp_packet_t *pkt, pgp_cbdata_t *cbinfo)
{
	const pgp_contents_t	*content = &pkt->u;
	pgp_keyring_t		*keyring;
	accumulate_t		*accumulate;

	if (pgp_get_debug_level(__FILE__)) {
		(void) fprintf(stderr, "accumulate callback: packet tag %u\n", pkt->tag);
	}
	accumulate = pgp_callback_arg(cbinfo);
	keyring = accumulate->keyring;
	switch (pkt->tag) {
	case PGP_PTAG_CT_PUBLIC_KEY:
	case PGP_PTAG_CT_PUBLIC_SUBKEY:
		pgp_add_to_pubring(keyring, &content->pubkey, pkt->tag);
		return PGP_KEEP_MEMORY;
	case PGP_PTAG_CT_SECRET_KEY:
	case PGP_PTAG_CT_ENCRYPTED_SECRET_KEY:
		pgp_add_to_secring(keyring, &content->seckey);
		return PGP_KEEP_MEMORY;
	case PGP_PTAG_CT_USER_ID:
		if (pgp_get_debug_level(__FILE__)) {
			(void) fprintf(stderr, "User ID: %s for key %d\n",
					content->userid,
					keyring->keyc - 1);
		}
		if (keyring->keyc == 0) {
			PGP_ERROR_1(cbinfo->errors, PGP_E_P_NO_USERID, "%s",
			    "No userid found");
		} else {
			pgp_add_userid(&keyring->keys[keyring->keyc - 1], content->userid);
		}
		return PGP_KEEP_MEMORY;
	case PGP_PARSER_PACKET_END:
		if (keyring->keyc > 0) {
			pgp_add_subpacket(&keyring->keys[keyring->keyc - 1],
						&content->packet);
			return PGP_KEEP_MEMORY;
		}
		return PGP_RELEASE_MEMORY;
	case PGP_PARSER_ERROR:
		(void) fprintf(stderr, "Error: %s\n", content->error);
		return PGP_FINISHED;
	case PGP_PARSER_ERRCODE:
		(void) fprintf(stderr, "parse error: %s\n",
				pgp_errcode(content->errcode.errcode));
		break;
	default:
		break;
	}
	/* XXX: we now exclude so many things, we should either drop this or */
	/* do something to pass on copies of the stuff we keep */
	return pgp_stacked_callback(pkt, cbinfo);
}

/**
 * \ingroup Core_Parse
 *
 * Parse packets from an input stream until EOF or error.
 *
 * Key data found in the parsed data is added to #keyring.
 *
 * \param keyring Pointer to an existing keyring
 * \param parse Options to use when parsing
*/
int 
pgp_parse_and_accumulate(pgp_keyring_t *keyring, pgp_stream_t *parse)
{
	accumulate_t	accumulate;
	const int	printerrors = 1;
	int             ret;

	if (parse->readinfo.accumulate) {
		(void) fprintf(stderr,
			"pgp_parse_and_accumulate: already init\n");
		return 0;
	}

	(void) memset(&accumulate, 0x0, sizeof(accumulate));

	accumulate.keyring = keyring;

	pgp_callback_push(parse, accumulate_cb, &accumulate);
	parse->readinfo.accumulate = 1;
	ret = pgp_parse(parse, !printerrors);

	return ret;
}


/** \file
 * \brief Error Handling
 */
#define ERRNAME(code)	{ code, #code }

static pgp_errcode_name_map_t errcode_name_map[] = {
	ERRNAME(PGP_E_OK),
	ERRNAME(PGP_E_FAIL),
	ERRNAME(PGP_E_SYSTEM_ERROR),
	ERRNAME(PGP_E_UNIMPLEMENTED),

	ERRNAME(PGP_E_R),
	ERRNAME(PGP_E_R_READ_FAILED),
	ERRNAME(PGP_E_R_EARLY_EOF),
	ERRNAME(PGP_E_R_BAD_FORMAT),
	ERRNAME(PGP_E_R_UNCONSUMED_DATA),

	ERRNAME(PGP_E_W),
	ERRNAME(PGP_E_W_WRITE_FAILED),
	ERRNAME(PGP_E_W_WRITE_TOO_SHORT),

	ERRNAME(PGP_E_P),
	ERRNAME(PGP_E_P_NOT_ENOUGH_DATA),
	ERRNAME(PGP_E_P_UNKNOWN_TAG),
	ERRNAME(PGP_E_P_PACKET_CONSUMED),
	ERRNAME(PGP_E_P_MPI_FORMAT_ERROR),

	ERRNAME(PGP_E_C),

	ERRNAME(PGP_E_V),
	ERRNAME(PGP_E_V_BAD_SIGNATURE),
	ERRNAME(PGP_E_V_NO_SIGNATURE),
	ERRNAME(PGP_E_V_UNKNOWN_SIGNER),

	ERRNAME(PGP_E_ALG),
	ERRNAME(PGP_E_ALG_UNSUPPORTED_SYMMETRIC_ALG),
	ERRNAME(PGP_E_ALG_UNSUPPORTED_PUBLIC_KEY_ALG),
	ERRNAME(PGP_E_ALG_UNSUPPORTED_SIGNATURE_ALG),
	ERRNAME(PGP_E_ALG_UNSUPPORTED_HASH_ALG),

	ERRNAME(PGP_E_PROTO),
	ERRNAME(PGP_E_PROTO_BAD_SYMMETRIC_DECRYPT),
	ERRNAME(PGP_E_PROTO_UNKNOWN_SS),
	ERRNAME(PGP_E_PROTO_CRITICAL_SS_IGNORED),
	ERRNAME(PGP_E_PROTO_BAD_PUBLIC_KEY_VRSN),
	ERRNAME(PGP_E_PROTO_BAD_SIGNATURE_VRSN),
	ERRNAME(PGP_E_PROTO_BAD_ONE_PASS_SIG_VRSN),
	ERRNAME(PGP_E_PROTO_BAD_PKSK_VRSN),
	ERRNAME(PGP_E_PROTO_DECRYPTED_MSG_WRONG_LEN),
	ERRNAME(PGP_E_PROTO_BAD_SK_CHECKSUM),

	{0x00, NULL},		/* this is the end-of-array marker */
};

/**
 * \ingroup Core_Errors
 * \brief returns error code name
 * \param errcode
 * \return error code name or "Unknown"
 */
const char     *
pgp_errcode(const pgp_errcode_t errcode)
{
	return (pgp_str_from_map((int) errcode,
			(pgp_map_t *) errcode_name_map));
}

/* generic grab new storage function */
void *
pgp_new(size_t size)
{
	void	*vp;

	if ((vp = calloc(1, size)) == NULL) {
		(void) fprintf(stderr,
			"allocation failure for %" PRIsize "u bytes", size);
	}
	return vp;
}

/**
 * \ingroup Core_Errors
 * \brief Pushes the given error on the given errorstack
 * \param errstack Error stack to use
 * \param errcode Code of error to push
 * \param sys_errno System errno (used if errcode=PGP_E_SYSTEM_ERROR)
 * \param file Source filename where error occurred
 * \param line Line in source file where error occurred
 * \param fmt Comment
 *
 */

void 
pgp_push_error(pgp_error_t **errstack, pgp_errcode_t errcode,
		int sys_errno, const char *file, int line, const char *fmt,...)
{
	/* first get the varargs and generate the comment */
	pgp_error_t  *err;
	unsigned	maxbuf = 128;
	va_list		args;
	char           *comment;

	if ((comment = calloc(1, maxbuf + 1)) == NULL) {
		(void) fprintf(stderr, "calloc comment failure\n");
		return;
	}

	va_start(args, fmt);
	vsnprintf(comment, maxbuf + 1, fmt, args);
	va_end(args);

	/* alloc a new error and add it to the top of the stack */

	if ((err = calloc(1, sizeof(*err))) == NULL) {
		(void) fprintf(stderr, "calloc comment failure\n");
		return;
	}

	err->next = *errstack;
	*errstack = err;

	/* fill in the details */
	err->errcode = errcode;
	err->sys_errno = sys_errno;
	err->file = file;
	err->line = line;

	err->comment = comment;
}

/**
\ingroup Core_Errors
\brief print this error
\param err Error to print
*/
void 
pgp_print_error(pgp_error_t *err)
{
	printf("%s:%d: ", err->file, err->line);
	if (err->errcode == PGP_E_SYSTEM_ERROR) {
		printf("system error %d returned from %s()\n", err->sys_errno,
		       err->comment);
	} else {
		printf("%s, %s\n", pgp_errcode(err->errcode), err->comment);
	}
}

/**
\ingroup Core_Errors
\brief Print all errors on stack
\param errstack Error stack to print
*/
void 
pgp_print_errors(pgp_error_t *errstack)
{
	pgp_error_t    *err;

	for (err = errstack; err != NULL; err = err->next) {
		pgp_print_error(err);
	}
}

/**
\ingroup Core_Errors
\brief Return 1 if given error is present anywhere on stack
\param errstack Error stack to check
\param errcode Error code to look for
\return 1 if found; else 0
*/
int 
pgp_has_error(pgp_error_t *errstack, pgp_errcode_t errcode)
{
	pgp_error_t    *err;

	for (err = errstack; err != NULL; err = err->next) {
		if (err->errcode == errcode) {
			return 1;
		}
	}
	return 0;
}

/**
\ingroup Core_Errors
\brief Frees all errors on stack
\param errstack Error stack to free
*/
void 
pgp_free_errors(pgp_error_t *errstack)
{
	pgp_error_t    *next;

	while (errstack != NULL) {
		next = errstack->next;
		free(errstack->comment);
		free(errstack);
		errstack = next;
	}
}

/* hash a 32-bit integer */
static int
hash_uint32(pgp_hash_t *hash, uint32_t n)
{
	uint8_t	ibuf[4];

	ibuf[0] = (uint8_t)(n >> 24) & 0xff;
	ibuf[1] = (uint8_t)(n >> 16) & 0xff;
	ibuf[2] = (uint8_t)(n >> 8) & 0xff;
	ibuf[3] = (uint8_t)n & 0xff;
	(*hash->add)(hash, (const uint8_t *)(void *)ibuf, (unsigned)sizeof(ibuf));
	return sizeof(ibuf);
}

/* hash a string - first length, then string itself */
static int
hash_string(pgp_hash_t *hash, const uint8_t *buf, uint32_t len)
{
	if (pgp_get_debug_level(__FILE__)) {
		hexdump(stderr, "hash_string", buf, len);
	}
	hash_uint32(hash, len);
	(*hash->add)(hash, buf, len);
	return (int)(sizeof(len) + len);
}

/* hash a bignum, possibly padded - first length, then string itself */
static int
hash_bignum(pgp_hash_t *hash, BIGNUM *bignum)
{
	uint8_t	*bn;
	size_t	 len;
	int	 padbyte;

	if (BN_is_zero(bignum)) {
		hash_uint32(hash, 0);
		return sizeof(len);
	}
	if ((len = (size_t) BN_num_bytes(bignum)) < 1) {
		(void) fprintf(stderr, "hash_bignum: bad size\n");
		return 0;
	}
	if ((bn = calloc(1, len)) == NULL) {
		(void) fprintf(stderr, "hash_bignum: bad bn alloc\n");
		return 0;
	}
	BN_bn2bin(bignum, bn + 1);
	bn[0] = 0x0;
	padbyte = (bn[1] & 0x80) ? 1 : 0;
	hash_string(hash, bn + 1 - padbyte, (unsigned)(len + padbyte));
	free(bn);
	return (int)(sizeof(len) + len + padbyte);
}

/** \file
 */

/**
 * \ingroup Core_Keys
 * \brief Calculate a public key fingerprint.
 * \param fp Where to put the calculated fingerprint
 * \param key The key for which the fingerprint is calculated
 */
int 
pgp_fingerprint(pgp_fingerprint_t *fp, const pgp_pubkey_t *key, pgp_hash_alg_t hashtype)
{
	pgp_memory_t	*mem;
	pgp_hash_t	 hash;
	const char	*type;
	uint32_t	 len;

	mem = pgp_memory_new();
	if (key->version == 2 || key->version == 3) {
		if (key->alg != PGP_PKA_RSA &&
		    key->alg != PGP_PKA_RSA_ENCRYPT_ONLY &&
		    key->alg != PGP_PKA_RSA_SIGN_ONLY) {
			(void) fprintf(stderr,
				"pgp_fingerprint: bad algorithm\n");
			return 0;
		}
		pgp_hash_md5(&hash);
		if (!hash.init(&hash)) {
			(void) fprintf(stderr,
				"pgp_fingerprint: bad md5 alloc\n");
			return 0;
		}
		hash_bignum(&hash, key->key.rsa.n);
		hash_bignum(&hash, key->key.rsa.e);
		fp->length = hash.finish(&hash, fp->fingerprint);
		if (pgp_get_debug_level(__FILE__)) {
			hexdump(stderr, "v2/v3 fingerprint", fp->fingerprint, fp->length);
		}
	} else if (hashtype == PGP_HASH_MD5) {
		pgp_hash_md5(&hash);
		if (!hash.init(&hash)) {
			(void) fprintf(stderr,
				"pgp_fingerprint: bad md5 alloc\n");
			return 0;
		}
		type = (key->alg == PGP_PKA_RSA) ? "ssh-rsa" : "ssh-dss";
		hash_string(&hash, (const uint8_t *)(const void *)type, (unsigned)strlen(type));
		switch(key->alg) {
		case PGP_PKA_RSA:
			hash_bignum(&hash, key->key.rsa.e);
			hash_bignum(&hash, key->key.rsa.n);
			break;
		case PGP_PKA_DSA:
			hash_bignum(&hash, key->key.dsa.p);
			hash_bignum(&hash, key->key.dsa.q);
			hash_bignum(&hash, key->key.dsa.g);
			hash_bignum(&hash, key->key.dsa.y);
			break;
		default:
			break;
		}
		fp->length = hash.finish(&hash, fp->fingerprint);
		if (pgp_get_debug_level(__FILE__)) {
			hexdump(stderr, "md5 fingerprint", fp->fingerprint, fp->length);
		}
	} else {
		pgp_build_pubkey(mem, key, 0);
		pgp_hash_sha1(&hash);
		if (!hash.init(&hash)) {
			(void) fprintf(stderr,
				"pgp_fingerprint: bad sha1 alloc\n");
			return 0;
		}
		len = (unsigned)pgp_mem_len(mem);
		pgp_hash_add_int(&hash, 0x99, 1);
		pgp_hash_add_int(&hash, len, 2);
		hash.add(&hash, pgp_mem_data(mem), len);
		fp->length = hash.finish(&hash, fp->fingerprint);
		pgp_memory_free(mem);
		if (pgp_get_debug_level(__FILE__)) {
			hexdump(stderr, "sha1 fingerprint", fp->fingerprint, fp->length);
		}
	}
	return 1;
}

/**
 * \ingroup Core_Keys
 * \brief Calculate the Key ID from the public key.
 * \param keyid Space for the calculated ID to be stored
 * \param key The key for which the ID is calculated
 */

int 
pgp_keyid(uint8_t *keyid, const size_t idlen, const pgp_pubkey_t *key, pgp_hash_alg_t hashtype)
{
	pgp_fingerprint_t finger;

	if (key->version == 2 || key->version == 3) {
		unsigned	n;
		uint8_t		bn[NETPGP_BUFSIZ];

		n = (unsigned) BN_num_bytes(key->key.rsa.n);
		if (n > sizeof(bn)) {
			(void) fprintf(stderr, "pgp_keyid: bad num bytes\n");
			return 0;
		}
		if (key->alg != PGP_PKA_RSA &&
		    key->alg != PGP_PKA_RSA_ENCRYPT_ONLY &&
		    key->alg != PGP_PKA_RSA_SIGN_ONLY) {
			(void) fprintf(stderr, "pgp_keyid: bad algorithm\n");
			return 0;
		}
		BN_bn2bin(key->key.rsa.n, bn);
		(void) memcpy(keyid, bn + n - idlen, idlen);
	} else {
		pgp_fingerprint(&finger, key, hashtype);
		(void) memcpy(keyid,
				finger.fingerprint + finger.length - idlen,
				idlen);
	}
	return 1;
}

/**
\ingroup Core_Hashes
\brief Add to the hash
\param hash Hash to add to
\param n Int to add
\param length Length of int in bytes
*/
void 
pgp_hash_add_int(pgp_hash_t *hash, unsigned n, unsigned length)
{
	uint8_t   c;

	while (length--) {
		c = n >> (length * 8);
		hash->add(hash, &c, 1);
	}
}

/**
\ingroup Core_Hashes
\brief Setup hash for given hash algorithm
\param hash Hash to set up
\param alg Hash algorithm to use
*/
void 
pgp_hash_any(pgp_hash_t *hash, pgp_hash_alg_t alg)
{
	switch (alg) {
	case PGP_HASH_MD5:
		pgp_hash_md5(hash);
		break;

	case PGP_HASH_SHA1:
		pgp_hash_sha1(hash);
		break;

	case PGP_HASH_SHA256:
		pgp_hash_sha256(hash);
		break;

	case PGP_HASH_SHA384:
		pgp_hash_sha384(hash);
		break;

	case PGP_HASH_SHA512:
		pgp_hash_sha512(hash);
		break;

	case PGP_HASH_SHA224:
		pgp_hash_sha224(hash);
		break;

	default:
		(void) fprintf(stderr, "pgp_hash_any: bad algorithm\n");
	}
}

/**
\ingroup Core_Hashes
\brief Returns size of hash for given hash algorithm
\param alg Hash algorithm to use
\return Size of hash algorithm in bytes
*/
unsigned 
pgp_hash_size(pgp_hash_alg_t alg)
{
	switch (alg) {
	case PGP_HASH_MD5:
		return 16;

	case PGP_HASH_SHA1:
		return 20;

	case PGP_HASH_SHA256:
		return 32;

	case PGP_HASH_SHA224:
		return 28;

	case PGP_HASH_SHA512:
		return 64;

	case PGP_HASH_SHA384:
		return 48;

	default:
		(void) fprintf(stderr, "pgp_hash_size: bad algorithm\n");
	}

	return 0;
}

/**
\ingroup Core_Hashes
\brief Returns hash enum corresponding to given string
\param hash Text name of hash algorithm i.e. "SHA1"
\returns Corresponding enum i.e. PGP_HASH_SHA1
*/
pgp_hash_alg_t 
pgp_str_to_hash_alg(const char *hash)
{
	if (hash == NULL) {
		return PGP_DEFAULT_HASH_ALGORITHM;
	}
	if (netpgp_strcasecmp(hash, "SHA1") == 0) {
		return PGP_HASH_SHA1;
	}
	if (netpgp_strcasecmp(hash, "MD5") == 0) {
		return PGP_HASH_MD5;
	}
	if (netpgp_strcasecmp(hash, "SHA256") == 0) {
		return PGP_HASH_SHA256;
	}
	/*
        if (netpgp_strcasecmp(hash,"SHA224") == 0) {
		return PGP_HASH_SHA224;
	}
        */
	if (netpgp_strcasecmp(hash, "SHA512") == 0) {
		return PGP_HASH_SHA512;
	}
	if (netpgp_strcasecmp(hash, "SHA384") == 0) {
		return PGP_HASH_SHA384;
	}
	return PGP_HASH_UNKNOWN;
}

/**
\ingroup Core_Hashes
\brief Hash given data
\param out Where to write the hash
\param alg Hash algorithm to use
\param in Data to hash
\param length Length of data
\return Size of hash created
*/
unsigned 
pgp_hash(uint8_t *out, pgp_hash_alg_t alg, const void *in, size_t length)
{
	pgp_hash_t      hash;

	pgp_hash_any(&hash, alg);
	if (!hash.init(&hash)) {
		(void) fprintf(stderr, "pgp_hash: bad alloc\n");
		/* we'll just continue here - don't want to return a 0 hash */
		/* XXX - agc - no way to return failure */
	}
	hash.add(&hash, in, (unsigned)length);
	return hash.finish(&hash, out);
}

/**
\ingroup Core_Hashes
\brief Calculate hash for MDC packet
\param preamble Preamble to hash
\param sz_preamble Size of preamble
\param plaintext Plaintext to hash
\param sz_plaintext Size of plaintext
\param hashed Resulting hash
*/
void 
pgp_calc_mdc_hash(const uint8_t *preamble,
			const size_t sz_preamble,
			const uint8_t *plaintext,
			const unsigned sz_plaintext,
			uint8_t *hashed)
{
	pgp_hash_t	hash;
	uint8_t		c;

	if (pgp_get_debug_level(__FILE__)) {
		hexdump(stderr, "preamble", preamble, sz_preamble);
		hexdump(stderr, "plaintext", plaintext, sz_plaintext);
	}
	/* init */
	pgp_hash_any(&hash, PGP_HASH_SHA1);
	if (!hash.init(&hash)) {
		(void) fprintf(stderr, "pgp_calc_mdc_hash: bad alloc\n");
		/* we'll just continue here - it will die anyway */
		/* agc - XXX - no way to return failure */
	}

	/* preamble */
	hash.add(&hash, preamble, (unsigned)sz_preamble);
	/* plaintext */
	hash.add(&hash, plaintext, sz_plaintext);
	/* MDC packet tag */
	c = MDC_PKT_TAG;
	hash.add(&hash, &c, 1);
	/* MDC packet len */
	c = PGP_SHA1_HASH_SIZE;
	hash.add(&hash, &c, 1);

	/* finish */
	hash.finish(&hash, hashed);

	if (pgp_get_debug_level(__FILE__)) {
		hexdump(stderr, "hashed", hashed, PGP_SHA1_HASH_SIZE);
	}
}

/**
\ingroup HighLevel_Supported
\brief Is this Hash Algorithm supported?
\param hash_alg Hash Algorithm to check
\return 1 if supported; else 0
*/
unsigned 
pgp_is_hash_alg_supported(const pgp_hash_alg_t *hash_alg)
{
	switch (*hash_alg) {
	case PGP_HASH_MD5:
	case PGP_HASH_SHA1:
	case PGP_HASH_SHA256:
		return 1;

	default:
		return 0;
	}
}

/* structure to map string to cipher def */
typedef struct str2cipher_t {
	const char	*s;	/* cipher name */
	pgp_symm_alg_t i;	/* cipher def */
} str2cipher_t;

static str2cipher_t	str2cipher[] = {
	{	"cast5",		PGP_SA_CAST5		},
	{	"idea",			PGP_SA_IDEA		},
	{	"aes128",		PGP_SA_AES_128		},
	{	"aes256",		PGP_SA_AES_256		},
	{	"camellia128",		PGP_SA_CAMELLIA_128	},
	{	"camellia256",		PGP_SA_CAMELLIA_256	},
	{	"tripledes",		PGP_SA_TRIPLEDES	},
	{	NULL,			0			}
};

/* convert from a string to a cipher definition */
pgp_symm_alg_t 
pgp_str_to_cipher(const char *cipher)
{
	str2cipher_t	*sp;

	for (sp = str2cipher ; cipher && sp->s ; sp++) {
		if (netpgp_strcasecmp(cipher, sp->s) == 0) {
			return sp->i;
		}
	}
	return PGP_SA_DEFAULT_CIPHER;
}

void 
pgp_random(void *dest, size_t length)
{
	RAND_bytes(dest, (int)length);
}

/**
\ingroup HighLevel_Memory
\brief Memory to initialise
\param mem memory to initialise
\param needed Size to initialise to
*/
void 
pgp_memory_init(pgp_memory_t *mem, size_t needed)
{
	uint8_t	*temp;

	mem->length = 0;
	if (mem->buf) {
		if (mem->allocated < needed) {
			if ((temp = realloc(mem->buf, needed)) == NULL) {
				(void) fprintf(stderr, "pgp_memory_init: bad alloc\n");
			} else {
				mem->buf = temp;
				mem->allocated = needed;
			}
		}
	} else {
		if ((mem->buf = calloc(1, needed)) == NULL) {
			(void) fprintf(stderr, "pgp_memory_init: bad alloc\n");
		} else {
			mem->allocated = needed;
		}
	}
}

/**
\ingroup HighLevel_Memory
\brief Pad memory to required length
\param mem Memory to use
\param length New size
*/
void 
pgp_memory_pad(pgp_memory_t *mem, size_t length)
{
	uint8_t	*temp;

	if (mem->allocated < mem->length) {
		(void) fprintf(stderr, "pgp_memory_pad: bad alloc in\n");
		return;
	}
	if (mem->allocated < mem->length + length) {
		mem->allocated = mem->allocated * 2 + length;
		temp = realloc(mem->buf, mem->allocated);
		if (temp == NULL) {
			(void) fprintf(stderr, "pgp_memory_pad: bad alloc\n");
		} else {
			mem->buf = temp;
		}
	}
	if (mem->allocated < mem->length + length) {
		(void) fprintf(stderr, "pgp_memory_pad: bad alloc out\n");
	}
}

/**
\ingroup HighLevel_Memory
\brief Add data to memory
\param mem Memory to which to add
\param src Data to add
\param length Length of data to add
*/
void 
pgp_memory_add(pgp_memory_t *mem, const uint8_t *src, size_t length)
{
	pgp_memory_pad(mem, length);
	(void) memcpy(mem->buf + mem->length, src, length);
	mem->length += length;
}

/* XXX: this could be refactored via the writer, but an awful lot of */
/* hoops to jump through for 2 lines of code! */
void 
pgp_memory_place_int(pgp_memory_t *mem, unsigned offset, unsigned n,
		     size_t length)
{
	if (mem->allocated < offset + length) {
		(void) fprintf(stderr,
			"pgp_memory_place_int: bad alloc\n");
	} else {
		while (length-- > 0) {
			mem->buf[offset++] = n >> (length * 8);
		}
	}
}

/**
 * \ingroup HighLevel_Memory
 * \brief Retains allocated memory and set length of stored data to zero.
 * \param mem Memory to clear
 * \sa pgp_memory_release()
 * \sa pgp_memory_free()
 */
void 
pgp_memory_clear(pgp_memory_t *mem)
{
	mem->length = 0;
}

/**
\ingroup HighLevel_Memory
\brief Free memory and associated data
\param mem Memory to free
\note This does not free mem itself
\sa pgp_memory_clear()
\sa pgp_memory_free()
*/
void 
pgp_memory_release(pgp_memory_t *mem)
{
	if (mem->mmapped) {
		(void) munmap(mem->buf, mem->length);
	} else {
		free(mem->buf);
	}
	mem->buf = NULL;
	mem->length = 0;
}

void 
pgp_memory_make_packet(pgp_memory_t *out, pgp_content_enum tag)
{
	size_t          extra;

	extra = (out->length < 192) ? 1 : (out->length < 8192 + 192) ? 2 : 5;
	pgp_memory_pad(out, extra + 1);
	memmove(out->buf + extra + 1, out->buf, out->length);

	out->buf[0] = PGP_PTAG_ALWAYS_SET | PGP_PTAG_NEW_FORMAT | tag;

	if (out->length < 192) {
		out->buf[1] = (uint8_t)out->length;
	} else if (out->length < 8192 + 192) {
		out->buf[1] = (uint8_t)((out->length - 192) >> 8) + 192;
		out->buf[2] = (uint8_t)(out->length - 192);
	} else {
		out->buf[1] = 0xff;
		out->buf[2] = (uint8_t)(out->length >> 24);
		out->buf[3] = (uint8_t)(out->length >> 16);
		out->buf[4] = (uint8_t)(out->length >> 8);
		out->buf[5] = (uint8_t)(out->length);
	}

	out->length += extra + 1;
}

/**
   \ingroup HighLevel_Memory
   \brief Create a new zeroed pgp_memory_t
   \return Pointer to new pgp_memory_t
   \note Free using pgp_memory_free() after use.
   \sa pgp_memory_free()
*/

pgp_memory_t   *
pgp_memory_new(void)
{
	return calloc(1, sizeof(pgp_memory_t));
}

/**
   \ingroup HighLevel_Memory
   \brief Free memory ptr and associated memory
   \param mem Memory to be freed
   \sa pgp_memory_release()
   \sa pgp_memory_clear()
*/

void 
pgp_memory_free(pgp_memory_t *mem)
{
	pgp_memory_release(mem);
	free(mem);
}

/**
   \ingroup HighLevel_Memory
   \brief Get length of data stored in pgp_memory_t struct
   \return Number of bytes in data
*/
size_t 
pgp_mem_len(const pgp_memory_t *mem)
{
	return mem->length;
}

/**
   \ingroup HighLevel_Memory
   \brief Get data stored in pgp_memory_t struct
   \return Pointer to data
*/
void *
pgp_mem_data(pgp_memory_t *mem)
{
	return mem->buf;
}

/* read a gile into an pgp_memory_t */
int
pgp_mem_readfile(pgp_memory_t *mem, const char *f)
{
	struct stat	 st;
	FILE		*fp;
	int		 cc;

	if ((fp = fopen(f, "rb")) == NULL) {
		(void) fprintf(stderr,
				"pgp_mem_readfile: can't open \"%s\"\n", f);
		return 0;
	}
	(void) fstat(fileno(fp), &st);
	mem->allocated = (size_t)st.st_size;
	mem->buf = mmap(NULL, mem->allocated, PROT_READ,
				MAP_PRIVATE | MAP_FILE, fileno(fp), 0);
	if (mem->buf == MAP_FAILED) {
		/* mmap failed for some reason - try to allocate memory */
		if ((mem->buf = calloc(1, mem->allocated)) == NULL) {
			(void) fprintf(stderr, "pgp_mem_readfile: calloc\n");
			(void) fclose(fp);
			return 0;
		}
		/* read into contents of mem */
		for (mem->length = 0 ;
		     (cc = (int)read(fileno(fp), &mem->buf[mem->length],
					(size_t)(mem->allocated - mem->length))) > 0 ;
		     mem->length += (size_t)cc) {
		}
	} else {
		mem->length = mem->allocated;
		mem->mmapped = 1;
	}
	(void) fclose(fp);
	return (mem->allocated == mem->length);
}

typedef struct {
	uint16_t  sum;
} sum16_t;


/**
 * Searches the given map for the given type.
 * Returns a human-readable descriptive string if found,
 * returns NULL if not found
 *
 * It is the responsibility of the calling function to handle the
 * error case sensibly (i.e. don't just print out the return string.
 *
 */
static const char *
str_from_map_or_null(int type, pgp_map_t *map)
{
	pgp_map_t      *row;

	for (row = map; row->string != NULL; row++) {
		if (row->type == type) {
			return row->string;
		}
	}
	return NULL;
}

/**
 * \ingroup Core_Print
 *
 * Searches the given map for the given type.
 * Returns a readable string if found, "Unknown" if not.
 */

const char     *
pgp_str_from_map(int type, pgp_map_t *map)
{
	const char     *str;

	str = str_from_map_or_null(type, map);
	return (str) ? str : "Unknown";
}

#define LINELEN	16

/* show hexadecimal/ascii dump */
void 
hexdump(FILE *fp, const char *header, const uint8_t *src, size_t length)
{
	size_t	i;
	char	line[LINELEN + 1];

	(void) fprintf(fp, "%s%s", (header) ? header : "", (header) ? "\n" : "");
	(void) fprintf(fp, "[%" PRIsize "u char%s]\n", length, (length == 1) ? "" : "s");
	for (i = 0 ; i < length ; i++) {
		if (i % LINELEN == 0) {
			(void) fprintf(fp, "%.5" PRIsize "u | ", i);
		}
		(void) fprintf(fp, "%.02x ", (uint8_t)src[i]);
		line[i % LINELEN] = (isprint(src[i])) ? src[i] : '.';
		if (i % LINELEN == LINELEN - 1) {
			line[LINELEN] = 0x0;
			(void) fprintf(fp, " | %s\n", line);
		}
	}
	if (i % LINELEN != 0) {
		for ( ; i % LINELEN != 0 ; i++) {
			(void) fprintf(fp, "   ");
			line[i % LINELEN] = ' ';
		}
		line[LINELEN] = 0x0;
		(void) fprintf(fp, " | %s\n", line);
	}
}

/**
 * \ingroup HighLevel_Functions
 * \brief Closes down OpenPGP::SDK.
 *
 * Close down OpenPGP:SDK, release any resources under the control of
 * the library. 
 */

void 
pgp_finish(void)
{
	pgp_crypto_finish();
}

static int 
sum16_reader(pgp_stream_t *stream, void *dest_, size_t length, pgp_error_t **errors,
	     pgp_reader_t *readinfo, pgp_cbdata_t *cbinfo)
{
	const uint8_t	*dest = dest_;
	sum16_t		*arg = pgp_reader_get_arg(readinfo);
	int		 r;
	int		 n;

	r = pgp_stacked_read(stream, dest_, length, errors, readinfo, cbinfo);
	if (r < 0) {
		return r;
	}
	for (n = 0; n < r; ++n) {
		arg->sum = (arg->sum + dest[n]) & 0xffff;
	}
	return r;
}

static void 
sum16_destroyer(pgp_reader_t *readinfo)
{
	free(pgp_reader_get_arg(readinfo));
}

/**
   \ingroup Internal_Readers_Sum16
   \param stream Parse settings
*/

void 
pgp_reader_push_sum16(pgp_stream_t *stream)
{
	sum16_t    *arg;

	if ((arg = calloc(1, sizeof(*arg))) == NULL) {
		(void) fprintf(stderr, "pgp_reader_push_sum16: bad alloc\n");
	} else {
		pgp_reader_push(stream, sum16_reader, sum16_destroyer, arg);
	}
}

/**
   \ingroup Internal_Readers_Sum16
   \param stream Parse settings
   \return sum
*/
uint16_t 
pgp_reader_pop_sum16(pgp_stream_t *stream)
{
	uint16_t	 sum;
	sum16_t		*arg;

	arg = pgp_reader_get_arg(pgp_readinfo(stream));
	sum = arg->sum;
	pgp_reader_pop(stream);
	free(arg);
	return sum;
}

/* small useful functions for setting the file-level debugging levels */
/* if the debugv list contains the filename in question, we're debugging it */

enum {
	MAX_DEBUG_NAMES = 32
};

static int      debugc;
static char    *debugv[MAX_DEBUG_NAMES];

/* set the debugging level per filename */
int
pgp_set_debug_level(const char *f)
{
	const char     *name;
	int             i;

	if (f == NULL) {
		f = "all";
	}
	if ((name = strrchr(f, '/')) == NULL) {
		name = f;
	} else {
		name += 1;
	}
	for (i = 0; i < debugc && i < MAX_DEBUG_NAMES; i++) {
		if (strcmp(debugv[i], name) == 0) {
			return 1;
		}
	}
	if (i == MAX_DEBUG_NAMES) {
		return 0;
	}
	debugv[debugc++] = netpgp_strdup(name);
	return 1;
}

/* get the debugging level per filename */
int
pgp_get_debug_level(const char *f)
{
	const char     *name;
	int             i;

	if ((name = strrchr(f, '/')) == NULL) {
		name = f;
	} else {
		name += 1;
	}
	for (i = 0; i < debugc; i++) {
		if (strcmp(debugv[i], "all") == 0 ||
		    strcmp(debugv[i], name) == 0) {
			return 1;
		}
	}
	return 0;
}

/* return the version for the library */
const char *
pgp_get_info(const char *type)
{
	if (strcmp(type, "version") == 0) {
		return NETPGP_VERSION_STRING;
	}
	if (strcmp(type, "maintainer") == 0) {
		return NETPGP_MAINTAINER;
	}
	return "[unknown]";
}

/* local version of asprintf so we don't have to play autoconf games */
int
pgp_asprintf(char **ret, const char *fmt, ...)
{
	va_list args;
	char    buf[120 * 1024];	/* XXX - "huge" buffer on stack */
	int     cc;

	va_start(args, fmt);
	cc = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	if ((*ret = calloc(1, (size_t)(cc + 1))) == NULL) {
		*ret = NULL;
		return -1;
	}
	(void) memcpy(*ret, buf, (size_t)cc);
	(*ret)[cc] = 0x0;
	return cc;
}

void
netpgp_log(const char *fmt, ...)
{
	va_list	 vp;
	time_t	 t;
	char	 buf[BUFSIZ * 2];
	int	 cc;

	(void) time(&t);
	cc = snprintf(buf, sizeof(buf), "%.24s: netpgp: ", ctime(&t));
	va_start(vp, fmt);
	(void) vsnprintf(&buf[cc], sizeof(buf) - (size_t)cc, fmt, vp);
	va_end(vp);
	/* do something with message */
	/* put into log buffer? */
}

/* portable replacement for strdup(3) */
char *
netpgp_strdup(const char *s)
{
	size_t	 len;
	char	*cp;

	len = strlen(s);
	if ((cp = calloc(1, len + 1)) != NULL) {
		(void) memcpy(cp, s, len);
		cp[len] = 0x0;
	}
	return cp;
}

/* portable replacement for strcasecmp(3) */
int
netpgp_strcasecmp(const char *s1, const char *s2)
{
	int	n;

	for (n = 0 ; *s1 && *s2 && (n = tolower((uint8_t)*s1) - tolower((uint8_t)*s2)) == 0 ; s1++, s2++) {
	}
	return n;
}
