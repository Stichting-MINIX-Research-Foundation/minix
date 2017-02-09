/*	$NetBSD: nist_ctr_drbg_aes256.h,v 1.2 2011/12/17 20:05:38 tls Exp $ */

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

#ifndef NIST_CTR_DRBG_AES256_H
#define NIST_CTR_DRBG_AES256_H

/* Choose AES-256 as the underlying block cipher */
#define NIST_BLOCK_KEYLEN		(256)
#define NIST_BLOCK_KEYLEN_BYTES	(NIST_BLOCK_KEYLEN / 8)
#define NIST_BLOCK_KEYLEN_INTS	(NIST_BLOCK_KEYLEN_BYTES / sizeof(int))

#define NIST_BLOCK_OUTLEN		(NIST_AES_BLOCKSIZEBITS)
#define NIST_BLOCK_OUTLEN_BYTES	(NIST_BLOCK_OUTLEN / 8)
#define NIST_BLOCK_OUTLEN_INTS	(NIST_BLOCK_OUTLEN_BYTES / sizeof(int))
#define NIST_BLOCK_OUTLEN_LONGS (NIST_BLOCK_OUTLEN_BYTES / sizeof(long))

typedef NIST_AES_ENCRYPT_CTX NIST_Key;

#define Block_Encrypt(ctx, src, dst) NIST_AES_ECB_Encrypt(ctx, src, dst)
#define Block_Schedule_Encryption(ctx, key) \
	NIST_AES_Schedule_Encryption(ctx, key, NIST_BLOCK_KEYLEN)

/*
 * NIST SP 800-90 March 2007
 * 10.2 DRBG Mechanism Based on Block Ciphers
 *
 * Table 3 specifies the reseed interval as
 * <= 2^48.  We use 2^31 so we can always be sure it'll fit in an int.
 */
#define NIST_CTR_DRBG_RESEED_INTERVAL   (0x7fffffffU)

#endif /* NIST_CTR_DRBG_AES256_H */
