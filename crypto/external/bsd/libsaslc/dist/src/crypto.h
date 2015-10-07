/* $NetBSD: crypto.h,v 1.3 2011/02/11 23:44:43 christos Exp $ */

/*
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Mateusz Kocielski.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

#ifndef _CRYPTO_H_
#define _CRYPTO_H_

/* hmac md5 digest length in ascii format */
#define HMAC_MD5_DIGEST_LENGTH	32

/* nonce/random functions */
char *saslc__crypto_nonce(size_t);

/* encoding functions */
int   saslc__crypto_decode_base64(const char *, size_t, void **, size_t *);
int   saslc__crypto_encode_base64(const void *, size_t, char **, size_t *);
char *saslc__crypto_hash_to_hex(const uint8_t *);

/* hashing functions */
void  saslc__crypto_md5_hash(const char *, size_t, unsigned char *);
char *saslc__crypto_md5_hex(const char *, size_t);
int   saslc__crypto_hmac_md5_hash(const unsigned char *, size_t,
    const unsigned char *, size_t, unsigned char *);
char *saslc__crypto_hmac_md5_hex(const unsigned char *, size_t,
    const unsigned char *, size_t);

#endif /* ! _CRYPTO_H_ */
