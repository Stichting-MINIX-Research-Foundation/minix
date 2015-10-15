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
#ifndef DIGEST_H_
#define DIGEST_H_	20100108

#include <sys/types.h>

#include <inttypes.h>

#include "md5.h"
#include "sha1.h"
#include "sha2.h"
#include "rmd160.h"
#include "tiger.h"

#ifndef __BEGIN_DECLS
#  if defined(__cplusplus)
#  define __BEGIN_DECLS           extern "C" {
#  define __END_DECLS             }
#  else
#  define __BEGIN_DECLS
#  define __END_DECLS
#  endif
#endif

__BEGIN_DECLS

#define MD5_HASH_ALG		1
#define SHA1_HASH_ALG		2
#define RIPEMD_HASH_ALG		3
#define TIGER_HASH_ALG		6	/* from rfc2440 */
#define SHA256_HASH_ALG		8
#define SHA384_HASH_ALG		9
#define SHA512_HASH_ALG		10
#define SHA224_HASH_ALG		11
#define TIGER2_HASH_ALG		100	/* private/experimental from rfc4880 */

/* structure to describe digest methods */
typedef struct digest_t {
	uint32_t		 alg;		/* algorithm */
	size_t			 size;		/* size */
	union {
		MD5_CTX		 md5ctx;	/* MD5 */
		SHA1_CTX	 sha1ctx;	/* SHA1 */
		RMD160_CTX	 rmd160ctx;	/* RIPEMD */
		SHA256_CTX	 sha256ctx;	/* SHA256 */
		SHA512_CTX	 sha512ctx;	/* SHA512 */
		TIGER_CTX	 tigerctx;	/* TIGER/TIGER2 */
	} u;
	void			*prefix;	/* points to specific prefix */
	uint32_t		 len;		/* prefix length */
	void			*ctx;		/* pointer to context array */
} digest_t;

unsigned digest_get_alg(const char */*hashalg*/);

int digest_init(digest_t */*digest*/, const uint32_t /*hashalg*/);

int digest_update(digest_t */*digest*/, const uint8_t */*data*/, size_t /*size*/);
unsigned digest_final(uint8_t */*out*/, digest_t */*digest*/);
int digest_alg_size(unsigned /*alg*/);
int digest_length(digest_t */*hash*/, unsigned /*hashedlen*/);

unsigned digest_get_prefix(unsigned /*hashalg*/, uint8_t */*prefix*/, size_t /*size*/);

__END_DECLS

#endif
