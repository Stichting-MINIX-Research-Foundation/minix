/*	$NetBSD: md2.c,v 1.7 2012/06/25 22:32:44 abs Exp $	*/

/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Brown.
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

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: md2.c,v 1.7 2012/06/25 22:32:44 abs Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#include <sys/types.h>

#include <assert.h>
#include <md2.h>
#include <string.h>

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#if !HAVE_MD2_H

/* cut-n-pasted from rfc1319 */
static unsigned char S[256] = {
	41, 46, 67, 201, 162, 216, 124, 1, 61, 54, 84, 161, 236, 240, 6,
	19, 98, 167, 5, 243, 192, 199, 115, 140, 152, 147, 43, 217, 188,
	76, 130, 202, 30, 155, 87, 60, 253, 212, 224, 22, 103, 66, 111, 24,
	138, 23, 229, 18, 190, 78, 196, 214, 218, 158, 222, 73, 160, 251,
	245, 142, 187, 47, 238, 122, 169, 104, 121, 145, 21, 178, 7, 63,
	148, 194, 16, 137, 11, 34, 95, 33, 128, 127, 93, 154, 90, 144, 50,
	39, 53, 62, 204, 231, 191, 247, 151, 3, 255, 25, 48, 179, 72, 165,
	181, 209, 215, 94, 146, 42, 172, 86, 170, 198, 79, 184, 56, 210,
	150, 164, 125, 182, 118, 252, 107, 226, 156, 116, 4, 241, 69, 157,
	112, 89, 100, 113, 135, 32, 134, 91, 207, 101, 230, 45, 168, 2, 27,
	96, 37, 173, 174, 176, 185, 246, 28, 70, 97, 105, 52, 64, 126, 15,
	85, 71, 163, 35, 221, 81, 175, 58, 195, 92, 249, 206, 186, 197,
	234, 38, 44, 83, 13, 110, 133, 40, 132, 9, 211, 223, 205, 244, 65,
	129, 77, 82, 106, 220, 55, 200, 108, 193, 171, 250, 36, 225, 123,
	8, 12, 189, 177, 74, 120, 136, 149, 139, 227, 99, 232, 109, 233,
	203, 213, 254, 59, 0, 29, 57, 242, 239, 183, 14, 102, 88, 208, 228,
	166, 119, 114, 248, 235, 117, 75, 10, 49, 68, 80, 180, 143, 237,
	31, 26, 219, 153, 141, 51, 159, 17, 131, 20
};

/* cut-n-pasted from rfc1319 */
static const unsigned char *pad[] = {
	(const unsigned char *)"",
	(const unsigned char *)"\001",
	(const unsigned char *)"\002\002",
	(const unsigned char *)"\003\003\003",
	(const unsigned char *)"\004\004\004\004",
	(const unsigned char *)"\005\005\005\005\005",
	(const unsigned char *)"\006\006\006\006\006\006",
	(const unsigned char *)"\007\007\007\007\007\007\007",
	(const unsigned char *)"\010\010\010\010\010\010\010\010",
	(const unsigned char *)"\011\011\011\011\011\011\011\011\011",
	(const unsigned char *)"\012\012\012\012\012\012\012\012\012\012",
	(const unsigned char *)"\013\013\013\013\013\013\013\013\013\013\013",
	(const unsigned char *)
	"\014\014\014\014\014\014\014\014\014\014\014\014",
	(const unsigned char *)
	"\015\015\015\015\015\015\015\015\015\015\015\015\015",
	(const unsigned char *)
	"\016\016\016\016\016\016\016\016\016\016\016\016\016\016",
	(const unsigned char *)
	"\017\017\017\017\017\017\017\017\017\017\017\017\017\017\017",
	(const unsigned char *)
	"\020\020\020\020\020\020\020\020\020\020\020\020\020\020\020\020"
};

/*
 * XXX This should not be visible, but due to an accident, it is
 * XXX so it must remain so.
 */
/*static*/ void MD2Transform __P((MD2_CTX *));

#ifdef __weak_alias
__weak_alias(MD2Init,_MD2Init)
__weak_alias(MD2Update,_MD2Update)
__weak_alias(MD2Final,_MD2Final)
__weak_alias(MD2Transform,_MD2Transform)
#endif

void
MD2Init(MD2_CTX *context)
{
	_DIAGASSERT(context != 0);

	context->i = 16;
	memset(&context->C[0], 0, sizeof(context->C));
	memset(&context->X[0], 0, sizeof(context->X));
}

void
MD2Update(MD2_CTX *context, const unsigned char *input, unsigned int inputLen)
{
	unsigned int idx, piece;

	_DIAGASSERT(context != 0);
	_DIAGASSERT(input != 0);

	for (idx = 0; idx < inputLen; idx += piece) {
		piece = 32 - context->i;
		if ((inputLen - idx) < piece)
			piece = inputLen - idx;
		memcpy(&context->X[context->i], &input[idx], (size_t)piece);
		if ((context->i += piece) == 32)
			MD2Transform(context); /* resets i */
	}
}

void
MD2Final(unsigned char digest[16], MD2_CTX *context)
{
	unsigned int padlen;

	_DIAGASSERT(digest != 0);
	_DIAGASSERT(context != 0);

	/* padlen should be 1..16 */
	padlen = 32 - context->i;

	/* add padding */
	MD2Update(context, pad[padlen], padlen);

	/* add checksum */
	MD2Update(context, &context->C[0], (unsigned int) sizeof(context->C));

	/* copy out final digest */
	memcpy(digest, &context->X[0], (size_t)16);

	/* reset the context */
	MD2Init(context);
}

/*static*/ void
MD2Transform(MD2_CTX *context)
{
	uint32_t l, j, k, t;

	/* set block "3" and update "checksum" */
	for (l = context->C[15], j = 0; j < 16; j++) {
		context->X[32 + j] = context->X[j] ^ context->X[16 + j];
		l = context->C[j] ^= S[context->X[16 + j] ^ l];
	}

	/* mangle input block */
	for (t = j = 0; j < 18; t = (t + j) % 256, j++)
		for (k = 0; k < 48; k++)
			t = context->X[k] = (context->X[k] ^ S[t]);

	/* reset input pointer */
	context->i = 16;
}

#endif /* !HAVE_MD2_H */
