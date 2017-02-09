/*	$NetBSD: nist_ctr_aes_rijndael.h,v 1.1 2011/11/19 22:51:22 tls Exp $ */

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
 * Interface adapter for Rijndael implmentation (for use by NIST SP 800-90 CTR_DRBG)
 */

#ifndef NIST_AES_RIJNDAEL_H
#define NIST_AES_RIJNDAEL_H

#include <crypto/rijndael/rijndael.h>

#define NIST_AES_MAXKEYBITS		256
#define NIST_AES_MAXKEYBYTES	(NIST_AES_MAXKEYBITS / 8)
#define NIST_AES_MAXKEYINTS	(NIST_AES_MAXKEYBYTES / sizeof(int))

#define NIST_AES_BLOCKSIZEBITS	128
#define NIST_AES_BLOCKSIZEBYTES	(NIST_AES_BLOCKSIZEBITS / 8)
#define NIST_AES_BLOCKSIZEINTS	(NIST_AES_BLOCKSIZEBYTES / sizeof(int))

typedef rijndael_ctx NIST_AES_ENCRYPT_CTX;

static inline void
NIST_AES_ECB_Encrypt(const NIST_AES_ENCRYPT_CTX *ctx,
		     const void *src, void* dst)
{
	rijndael_encrypt(ctx, src, dst);
}

static inline int
NIST_AES_Schedule_Encryption(NIST_AES_ENCRYPT_CTX *ctx,
			     const void *key, int bits)
{
	rijndael_set_key(ctx, key, bits);
	return 0;
}

#endif /* NIST_AES_RIJNDAEL_H */
