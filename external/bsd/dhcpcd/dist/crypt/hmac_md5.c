#include <sys/cdefs.h>
 __RCSID("$NetBSD: hmac_md5.c,v 1.7 2015/03/27 11:33:47 roy Exp $");

/*
 * dhcpcd - DHCP client daemon
 * Copyright (c) 2006-2015 Roy Marples <roy@marples.name>
 * All rights reserved

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <inttypes.h>
#include <string.h>

#include "crypt.h"

#include "../config.h"
#ifdef HAVE_MD5_H
#  ifndef DEPGEN
#    include <md5.h>
#  endif
#else
#  include "md5.h"
#endif

#define HMAC_PAD_LEN	64
#define IPAD		0x36
#define OPAD		0x5C

/* hmac_md5 as per RFC3118 */
void
hmac_md5(const uint8_t *text, size_t text_len,
    const uint8_t *key, size_t key_len,
    uint8_t *digest)
{
	uint8_t k_ipad[HMAC_PAD_LEN], k_opad[HMAC_PAD_LEN];
	uint8_t tk[MD5_DIGEST_LENGTH];
	int i;
	MD5_CTX context;

	/* Ensure key is no bigger than HMAC_PAD_LEN */
	if (key_len > HMAC_PAD_LEN) {
		MD5Init(&context);
		MD5Update(&context, key, (unsigned int)key_len);
		MD5Final(tk, &context);
		key = tk;
		key_len = MD5_DIGEST_LENGTH;
	}

	/* store key in pads */
	memcpy(k_ipad, key, key_len);
	memcpy(k_opad, key, key_len);
	memset(k_ipad + key_len, 0, sizeof(k_ipad) - key_len);
	memset(k_opad + key_len, 0, sizeof(k_opad) - key_len);

	/* XOR key with ipad and opad values */
	for (i = 0; i < HMAC_PAD_LEN; i++) {
		k_ipad[i] ^= IPAD;
		k_opad[i] ^= OPAD;
	}

	/* inner MD5 */
	MD5Init(&context);
	MD5Update(&context, k_ipad, HMAC_PAD_LEN);
	MD5Update(&context, text, (unsigned int)text_len);
	MD5Final(digest, &context);

	/* outer MD5 */
	MD5Init(&context);
	MD5Update(&context, k_opad, HMAC_PAD_LEN);
	MD5Update(&context, digest, MD5_DIGEST_LENGTH);
	MD5Final(digest, &context);
}
