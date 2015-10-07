/*-
 * Copyright (c) 2012 Alistair Crooks <agc@NetBSD.org>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/syslog.h>

#ifdef _KERNEL
# include <sys/md5.h>
# include <sys/sha1.h>
# include <sys/sha2.h>
# include <sys/rmd160.h>
# include <sys/kmem.h>
#else
# include <arpa/inet.h>
# include <ctype.h>
# include <inttypes.h>
# include <md5.h>
# include <rmd160.h>
# include <sha1.h>
# include <sha2.h>
# include <stdarg.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <time.h>
# include <unistd.h>
#endif

#include "digest.h"

static uint8_t prefix_md5[] = {
	0x30, 0x20, 0x30, 0x0C, 0x06, 0x08, 0x2A, 0x86, 0x48, 0x86,
	0xF7, 0x0D, 0x02, 0x05, 0x05, 0x00, 0x04, 0x10
};

static uint8_t prefix_sha1[] = {
	0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0E, 0x03, 0x02,
	0x1A, 0x05, 0x00, 0x04, 0x14
};

static uint8_t prefix_sha256[] = {
	0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
	0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20
};

static uint64_t	prefix_tiger[] = {
	0x0123456789ABCDEFLL,
	0xFEDCBA9876543210LL,
	0xF096A5B4C3B2E187LL
};

static uint8_t prefix_rmd160[] = {
	0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2B, 0x24,
	0x03, 0x02, 0x01, 0x05, 0x00, 0x04, 0x14
};

static uint8_t prefix_sha512[] = {
	0x30, 0x51, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
	0x65, 0x03, 0x04, 0x02, 0x03, 0x05, 0x00, 0x04, 0x40
};

#define V4_SIGNATURE		4

/*************************************************************************/

void                                                                                                                 
MD5_Init(MD5_CTX *context)
{
	if (context) {
		MD5Init(context);
	}
}

void                                                                                                                 
MD5_Update(MD5_CTX *context, const unsigned char *data, unsigned int len)
{
	if (context && data) {
		MD5Update(context, data, len);
	}
}

void                                                                                                                 
MD5_Final(unsigned char digest[16], MD5_CTX *context)
{
	if (digest && context) {
		MD5Final(digest, context);
	}
}

void                                                                                                                 
SHA1_Init(SHA1_CTX *context)
{
	if (context) {
		SHA1Init(context);
	}
}

void                                                                                                                 
SHA1_Update(SHA1_CTX *context, const unsigned char *data, unsigned int len)
{
	if (context && data) {
		SHA1Update(context, data, len);
	}
}

void                                                                                                                 
SHA1_Final(unsigned char digest[20], SHA1_CTX *context)
{
	if (digest && context) {
		SHA1Final(digest, context);
	}
}

void                                                                                                                 
RMD160_Init(RMD160_CTX *context)
{
	if (context) {
		RMD160Init(context);
	}
}

void                                                                                                                 
RMD160_Update(RMD160_CTX *context, const unsigned char *data, unsigned int len)
{
	if (context && data) {
		RMD160Update(context, data, len);
	}
}

void                                                                                                                 
RMD160_Final(unsigned char digest[20], RMD160_CTX *context)
{
	if (context && digest) {
		RMD160Final(digest, context);
	}
}


/* algorithm size (raw) */
int 
digest_alg_size(unsigned alg)
{
	switch(alg) {
	case MD5_HASH_ALG:
		return 16;
	case SHA1_HASH_ALG:
		return 20;
	case RIPEMD_HASH_ALG:
		return RMD160_DIGEST_LENGTH;
	case SHA256_HASH_ALG:
		return 32;
	case SHA512_HASH_ALG:
		return 64;
	case TIGER_HASH_ALG:
	case TIGER2_HASH_ALG:
		return TIGER_DIGEST_LENGTH;
	default:
		printf("hash_any: bad algorithm\n");
		return 0;
	}
}

/* initialise the hash structure */
int 
digest_init(digest_t *hash, const uint32_t hashalg)
{
	if (hash == NULL) {
		return 0;
	}
	switch(hash->alg = hashalg) {
	case MD5_HASH_ALG:
		MD5Init(&hash->u.md5ctx);
		hash->size = 16;
		hash->prefix = prefix_md5;
		hash->len = sizeof(prefix_md5);
		hash->ctx = &hash->u.md5ctx;
		return 1;
	case SHA1_HASH_ALG:
		SHA1Init(&hash->u.sha1ctx);
		hash->size = 20;
		hash->prefix = prefix_sha1;
		hash->len = sizeof(prefix_sha1);
		hash->ctx = &hash->u.sha1ctx;
		return 1;
	case RIPEMD_HASH_ALG:
		RMD160Init(&hash->u.rmd160ctx);
		hash->size = 20;
		hash->prefix = prefix_rmd160;
		hash->len = sizeof(prefix_rmd160);
		hash->ctx = &hash->u.rmd160ctx;
		return 1;
	case SHA256_HASH_ALG:
		SHA256_Init(&hash->u.sha256ctx);
		hash->size = 32;
		hash->prefix = prefix_sha256;
		hash->len = sizeof(prefix_sha256);
		hash->ctx = &hash->u.sha256ctx;
		return 1;
	case SHA512_HASH_ALG:
		SHA512_Init(&hash->u.sha512ctx);
		hash->size = 64;
		hash->prefix = prefix_sha512;
		hash->len = sizeof(prefix_sha512);
		hash->ctx = &hash->u.sha512ctx;
		return 1;
	case TIGER_HASH_ALG:
		TIGER_Init(&hash->u.tigerctx);
		hash->size = TIGER_DIGEST_LENGTH;
		hash->prefix = prefix_tiger;
		hash->len = sizeof(prefix_tiger);
		hash->ctx = &hash->u.tigerctx;
		return 1;
	case TIGER2_HASH_ALG:
		TIGER2_Init(&hash->u.tigerctx);
		hash->size = TIGER_DIGEST_LENGTH;
		hash->prefix = prefix_tiger;
		hash->len = sizeof(prefix_tiger);
		hash->ctx = &hash->u.tigerctx;
		return 1;
	default:
		printf("hash_any: bad algorithm\n");
		return 0;
	}
}

typedef struct rec_t {
	const char	*s;
	const unsigned	 alg;
} rec_t;

static rec_t	hashalgs[] = {
	{	"md5",		MD5_HASH_ALG	},
	{	"sha1",		SHA1_HASH_ALG	},
	{	"ripemd",	RIPEMD_HASH_ALG	},
	{	"sha256",	SHA256_HASH_ALG	},
	{	"sha512",	SHA512_HASH_ALG	},
	{	"tiger",	TIGER_HASH_ALG	},
	{	"tiger2",	TIGER2_HASH_ALG	},
	{	NULL,		0		}
};

/* initialise by string alg name */
unsigned 
digest_get_alg(const char *hashalg)
{
	rec_t	*r;

	for (r = hashalgs ; hashalg && r->s ; r++) {
		if (strcasecmp(r->s, hashalg) == 0) {
			return r->alg;
		}
	}
	return 0;
}

int 
digest_update(digest_t *hash, const uint8_t *data, size_t length)
{
	if (hash == NULL || data == NULL) {
		return 0;
	}
	switch(hash->alg) {
	case MD5_HASH_ALG:
		MD5Update(hash->ctx, data, (unsigned)length);
		return 1;
	case SHA1_HASH_ALG:
		SHA1Update(hash->ctx, data, (unsigned)length);
		return 1;
	case RIPEMD_HASH_ALG:
		RMD160Update(hash->ctx, data, (unsigned)length);
		return 1;
	case SHA256_HASH_ALG:
		SHA256_Update(hash->ctx, data, length);
		return 1;
	case SHA512_HASH_ALG:
		SHA512_Update(hash->ctx, data, length);
		return 1;
	case TIGER_HASH_ALG:
	case TIGER2_HASH_ALG:
		TIGER_Update(hash->ctx, data, length);
		return 1;
	default:
		printf("hash_any: bad algorithm\n");
		return 0;
	}
}

unsigned 
digest_final(uint8_t *out, digest_t *hash)
{
	if (hash == NULL || out == NULL) {
		return 0;
	}
	switch(hash->alg) {
	case MD5_HASH_ALG:
		MD5Final(out, hash->ctx);
		break;
	case SHA1_HASH_ALG:
		SHA1Final(out, hash->ctx);
		break;
	case RIPEMD_HASH_ALG:
		RMD160Final(out, hash->ctx);
		break;
	case SHA256_HASH_ALG:
		SHA256_Final(out, hash->ctx);
		break;
	case SHA512_HASH_ALG:
		SHA512_Final(out, hash->ctx);
		break;
	case TIGER_HASH_ALG:
		TIGER_Final(out, hash->ctx);
		break;
	default:
		printf("hash_any: bad algorithm\n");
		return 0;
	}
	(void) memset(hash->ctx, 0x0, hash->size);
	return (unsigned)hash->size;
}

int
digest_length(digest_t *hash, unsigned hashedlen)
{
	uint8_t		 trailer[6];

	if (hash == NULL) {
		return 0;
	}
	trailer[0] = V4_SIGNATURE;
	trailer[1] = 0xFF;
	trailer[2] = (uint8_t)((hashedlen >> 24) & 0xff);
	trailer[3] = (uint8_t)((hashedlen >> 16) & 0xff);
	trailer[4] = (uint8_t)((hashedlen >> 8) & 0xff);
	trailer[5] = (uint8_t)(hashedlen & 0xff);
	digest_update(hash, trailer, sizeof(trailer));
	return 1;
}

unsigned
digest_get_prefix(unsigned hashalg, uint8_t *prefix, size_t size)
{
	if (prefix == NULL) {
		return 0;
	}
	switch (hashalg) {
	case MD5_HASH_ALG:
		memcpy(prefix, prefix_md5, sizeof(prefix_md5));
		return sizeof(prefix_md5);
	case SHA1_HASH_ALG:
		memcpy(prefix, prefix_sha1, sizeof(prefix_sha1));
		return sizeof(prefix_sha1);
	case SHA256_HASH_ALG:
		memcpy(prefix, prefix_sha256, sizeof(prefix_sha256));
		return sizeof(prefix_sha256);
	default:
		printf("digest_get_prefix: unknown hash algorithm: %d\n", hashalg);
		return 0;
	}
}

