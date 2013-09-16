/*	$NetBSD: cprng.h,v 1.5 2012/04/17 02:50:39 tls Exp $ */

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Thor Lancelot Simon.
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
#ifndef _CPRNG_H
#define _CPRNG_H

#include <sys/types.h>
#include <sys/fcntl.h>
#include <lib/libkern/libkern.h>
#include <sys/rnd.h>
#include <crypto/nist_ctr_drbg/nist_ctr_drbg.h>
#include <sys/condvar.h>
#include <sys/select.h>

/*
 * NIST SP800-90 says 2^19 bytes per request for the CTR_DRBG.
 */
#define CPRNG_MAX_LEN	524288

#if !defined(_RUMPKERNEL) && !defined(_RUMP_NATIVE_ABI)
/*
 * We do not want an arc4random() prototype available to anyone.
 */
void _arc4randbytes(void *, size_t);
uint32_t _arc4random(void);

static inline size_t
cprng_fast(void *p, size_t len)
{
	_arc4randbytes(p, len);
	return len;
}

static inline uint32_t
cprng_fast32(void)
{
	return _arc4random();
}

static inline uint64_t
cprng_fast64(void)
{
	uint64_t r;
	_arc4randbytes(&r, sizeof(r));
	return r;
}
#else
size_t cprng_fast(void *, size_t);
uint32_t cprng_fast32(void);
uint64_t cprng_fast64(void);
#endif

typedef struct _cprng_strong {
	kmutex_t	mtx;
	kcondvar_t	cv;
	struct selinfo	selq;
	NIST_CTR_DRBG	drbg;
	int		flags;
	char		name[16];
	int		reseed_pending;
	int		entropy_serial;
	rndsink_t	reseed;
} cprng_strong_t;

#define CPRNG_INIT_ANY		0x00000001
#define CPRNG_REKEY_ANY		0x00000002
#define CPRNG_USE_CV		0x00000004

cprng_strong_t *cprng_strong_create(const char *const, int, int);

size_t cprng_strong(cprng_strong_t *const, void *const, size_t, int);

void cprng_strong_destroy(cprng_strong_t *);

extern cprng_strong_t *	kern_cprng;

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

static inline int
cprng_strong_ready(cprng_strong_t *c)
{
	int ret = 0;
	
	mutex_enter(&c->mtx);
	if (c->drbg.reseed_counter < NIST_CTR_DRBG_RESEED_INTERVAL) {
		ret = 1;
	}
	mutex_exit(&c->mtx);
	return ret;
}

static inline void
cprng_strong_deplete(cprng_strong_t *c)
{
	mutex_enter(&c->mtx);
	c->drbg.reseed_counter = NIST_CTR_DRBG_RESEED_INTERVAL + 1;
	mutex_exit(&c->mtx);
}

static inline int
cprng_strong_strength(cprng_strong_t *c)
{
	return NIST_BLOCK_KEYLEN_BYTES;
}

void cprng_init(void);
int cprng_strong_getflags(cprng_strong_t *const);
void cprng_strong_setflags(cprng_strong_t *const, int);

#endif
