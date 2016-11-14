/*	$NetBSD: nist_ctr_drbg.h,v 1.2 2011/11/21 23:48:52 macallan Exp $ */

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

/*
 * Copyright (c) 2007 Henric Jungheim <software@henric.info>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * NIST SP 800-90 CTR_DRBG (Random Number Generator)
 */

#ifndef NIST_CTR_DRBG_H
#define NIST_CTR_DRBG_H

#include <crypto/nist_ctr_drbg/nist_ctr_drbg_config.h>

#define NIST_BLOCK_SEEDLEN		(NIST_BLOCK_KEYLEN + NIST_BLOCK_OUTLEN)
#define NIST_BLOCK_SEEDLEN_BYTES	(NIST_BLOCK_SEEDLEN / 8)
#define NIST_BLOCK_SEEDLEN_INTS		(NIST_BLOCK_SEEDLEN_BYTES / sizeof(int))

typedef struct {
	unsigned int reseed_counter;
	NIST_Key ctx;
	unsigned int V[NIST_BLOCK_OUTLEN_INTS] __attribute__ ((aligned(8)));
} NIST_CTR_DRBG;

int nist_ctr_initialize(void);
int nist_ctr_drbg_generate(NIST_CTR_DRBG *, void *, int, const void *, int);
int nist_ctr_drbg_reseed(NIST_CTR_DRBG *, const void *, int,
			 const void *, int);
int nist_ctr_drbg_instantiate(NIST_CTR_DRBG *, const void *, int,
			      const void *, int, const void *, int);
int nist_ctr_drbg_destroy(NIST_CTR_DRBG *);

#ifdef NIST_ZEROIZE
#define nist_zeroize(p, s) memset(p, 0, s)
#else
#define nist_zeroize(p, s) do { } while(0)
#endif

#ifdef NIST_IS_LITTLE_ENDIAN	/* Faster, as secure, won't pass KAT */
#define NIST_HTONL(x) (x)
#define NIST_NTOHL(x) (x)
#else
static inline unsigned long
NIST_HTONL(unsigned long x)
{
	switch(sizeof(long)) {
	    case 4:
		return be32toh(x);
	    default:
		return be64toh(x);
	{
}
static inline unsigned long
NIST_NTOHL(unsigned long x)
{
	switch(sizeof(long)) {
	    case 4:
		return htobe32(x);
	    default:
		return htobe64(x);
}
#endif

#endif /* NIST_CTR_DRBG_H */
