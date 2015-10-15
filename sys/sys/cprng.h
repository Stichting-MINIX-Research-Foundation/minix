/*	$NetBSD: cprng.h,v 1.12 2015/04/13 15:51:30 riastradh Exp $ */

/*-
 * Copyright (c) 2011-2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Thor Lancelot Simon and Taylor R. Campbell.
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
 * XXX Don't change this to _SYS_CPRNG_H or anything -- code outside
 * this file relies on its name...  (I'm looking at you, ipf!)
 */
#ifndef _CPRNG_H
#define _CPRNG_H

#include <sys/types.h>

#include <crypto/nist_ctr_drbg/nist_ctr_drbg.h>
#include <crypto/cprng_fast/cprng_fast.h>

/*
 * NIST SP800-90 says 2^19 bytes per request for the CTR_DRBG.
 */
#define CPRNG_MAX_LEN	524288

typedef struct cprng_strong cprng_strong_t;

void	cprng_init(void);

#define CPRNG_INIT_ANY		0x00000001
#define CPRNG_REKEY_ANY		0x00000002
#define CPRNG_USE_CV		0x00000004
#define CPRNG_HARD		0x00000008
#define CPRNG_FMT	"\177\020\
b\0INIT_ANY\0\
b\1REKEY_ANY\0\
b\2USE_CV\0\
b\3HARD\0"

cprng_strong_t *
	cprng_strong_create(const char *, int, int);
void	cprng_strong_destroy(cprng_strong_t *);
size_t	cprng_strong(cprng_strong_t *, void *, size_t, int);

struct knote;			/* XXX temp, for /dev/random */
int	cprng_strong_kqfilter(cprng_strong_t *, struct knote *); /* XXX " */
int	cprng_strong_poll(cprng_strong_t *, int); /* XXX " */

extern cprng_strong_t	*kern_cprng;

static inline uint32_t
cprng_strong32(void)
{
	uint32_t r;
	cprng_strong(kern_cprng, &r, sizeof(r), 0);
	return r;
}

static inline uint64_t
cprng_strong64(void)
{
	uint64_t r;
	cprng_strong(kern_cprng, &r, sizeof(r), 0);
	return r;
}

static inline unsigned int
cprng_strong_strength(cprng_strong_t *c)
{
	return NIST_BLOCK_KEYLEN_BYTES;
}

#endif	/* _CPRNG_H */
