/* $NetBSD: gmac.c,v 1.3 2011/06/09 14:47:42 drochner Exp $ */
/* OpenBSD: gmac.c,v 1.3 2011/01/11 15:44:23 deraadt Exp */

/*
 * Copyright (c) 2010 Mike Belopuhov <mike@vantronix.net>
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
 * This code implements the Message Authentication part of the
 * Galois/Counter Mode (as being described in the RFC 4543) using
 * the AES cipher.  FIPS SP 800-38D describes the algorithm details.
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <crypto/rijndael/rijndael.h>
#include <opencrypto/gmac.h>

void	ghash_gfmul(const GMAC_INT *, const GMAC_INT *, GMAC_INT *);
void	ghash_update(GHASH_CTX *, const uint8_t *, size_t);

/* Computes a block multiplication in the GF(2^128) */
void
ghash_gfmul(const GMAC_INT *X, const GMAC_INT *Y, GMAC_INT *product)
{
	GMAC_INT	v[GMAC_BLOCK_LEN/GMAC_INTLEN];
	uint32_t	mul;
	int		i;

	memcpy(v, Y, GMAC_BLOCK_LEN);
	memset(product, 0, GMAC_BLOCK_LEN);

	for (i = 0; i < GMAC_BLOCK_LEN * 8; i++) {
		/* update Z */
#if GMAC_INTLEN == 8
		if (X[i >> 6] & (1ULL << (~i & 63))) {
			product[0] ^= v[0];
			product[1] ^= v[1];
		} /* else: we preserve old values */
#else
		if (X[i >> 5] & (1 << (~i & 31))) {
			product[0] ^= v[0];
			product[1] ^= v[1];
			product[2] ^= v[2];
			product[3] ^= v[3];
		} /* else: we preserve old values */
#endif
		/* update V */
#if GMAC_INTLEN == 8
		mul = v[1] & 1;
		v[1] = (v[0] << 63) | (v[1] >> 1);
		v[0] = (v[0] >> 1) ^ (0xe100000000000000ULL * mul);
#else
		mul = v[3] & 1;
		v[3] = (v[2] << 31) | (v[3] >> 1);
		v[2] = (v[1] << 31) | (v[2] >> 1);
		v[1] = (v[0] << 31) | (v[1] >> 1);
		v[0] = (v[0] >> 1) ^ (0xe1000000 * mul);
#endif
	}
}

void
ghash_update(GHASH_CTX *ctx, const uint8_t *X, size_t len)
{
	GMAC_INT x;
	GMAC_INT *s = ctx->S;
	GMAC_INT *y = ctx->Z;
	int i, j, k;

	for (i = 0; i < len / GMAC_BLOCK_LEN; i++) {
		for (j = 0; j < GMAC_BLOCK_LEN/GMAC_INTLEN; j++) {
			x = 0;
			for (k = 0; k < GMAC_INTLEN; k++) {
				x <<= 8;
				x |= X[k];
			}
			s[j] = y[j] ^ x;
			X += GMAC_INTLEN;
		}

		ghash_gfmul(ctx->H, ctx->S, ctx->S);

		y = s;
	}

	memcpy(ctx->Z, ctx->S, GMAC_BLOCK_LEN);
}

#define AESCTR_NONCESIZE	4

void
AES_GMAC_Init(AES_GMAC_CTX *ctx)
{

	memset(ctx, 0, sizeof(AES_GMAC_CTX));
}

void
AES_GMAC_Setkey(AES_GMAC_CTX *ctx, const uint8_t *key, uint16_t klen)
{
	int i;

	ctx->rounds = rijndaelKeySetupEnc(ctx->K, (const u_char *)key,
	    (klen - AESCTR_NONCESIZE) * 8);
	/* copy out salt to the counter block */
	memcpy(ctx->J, key + klen - AESCTR_NONCESIZE, AESCTR_NONCESIZE);
	/* prepare a hash subkey */
	rijndaelEncrypt(ctx->K, ctx->rounds, (void *)ctx->ghash.H,
			(void *)ctx->ghash.H);
#if GMAC_INTLEN == 8
	for (i = 0; i < 2; i++)
		ctx->ghash.H[i] = be64toh(ctx->ghash.H[i]);
#else
	for (i = 0; i < 4; i++)
		ctx->ghash.H[i] = be32toh(ctx->ghash.H[i]);
#endif
}

void
AES_GMAC_Reinit(AES_GMAC_CTX *ctx, const uint8_t *iv, uint16_t ivlen)
{
	/* copy out IV to the counter block */
	memcpy(ctx->J + AESCTR_NONCESIZE, iv, ivlen);
}

int
AES_GMAC_Update(AES_GMAC_CTX *ctx, const uint8_t *data, uint16_t len)
{
	uint8_t		blk[16] = { 0 };
	int		plen;

	if (len > 0) {
		plen = len % GMAC_BLOCK_LEN;
		if (len >= GMAC_BLOCK_LEN)
			ghash_update(&ctx->ghash, data, len - plen);
		if (plen) {
			memcpy(blk, data + (len - plen), plen);
			ghash_update(&ctx->ghash, blk, GMAC_BLOCK_LEN);
		}
	}
	return (0);
}

void
AES_GMAC_Final(uint8_t digest[GMAC_DIGEST_LEN], AES_GMAC_CTX *ctx)
{
	uint8_t		keystream[GMAC_BLOCK_LEN], *k, *d;
	int		i;

	/* do one round of GCTR */
	ctx->J[GMAC_BLOCK_LEN - 1] = 1;
	rijndaelEncrypt(ctx->K, ctx->rounds, ctx->J, keystream);
	k = keystream;
	d = digest;
#if GMAC_INTLEN == 8
	for (i = 0; i < GMAC_DIGEST_LEN/8; i++) {
		d[0] = (uint8_t)(ctx->ghash.S[i] >> 56) ^ k[0];
		d[1] = (uint8_t)(ctx->ghash.S[i] >> 48) ^ k[1];
		d[2] = (uint8_t)(ctx->ghash.S[i] >> 40) ^ k[2];
		d[3] = (uint8_t)(ctx->ghash.S[i] >> 32) ^ k[3];
		d[4] = (uint8_t)(ctx->ghash.S[i] >> 24) ^ k[4];
		d[5] = (uint8_t)(ctx->ghash.S[i] >> 16) ^ k[5];
		d[6] = (uint8_t)(ctx->ghash.S[i] >> 8) ^ k[6];
		d[7] = (uint8_t)ctx->ghash.S[i] ^ k[7];
		d += 8;
		k += 8;
	}
#else
	for (i = 0; i < GMAC_DIGEST_LEN/4; i++) {
		d[0] = (uint8_t)(ctx->ghash.S[i] >> 24) ^ k[0];
		d[1] = (uint8_t)(ctx->ghash.S[i] >> 16) ^ k[1];
		d[2] = (uint8_t)(ctx->ghash.S[i] >> 8) ^ k[2];
		d[3] = (uint8_t)ctx->ghash.S[i] ^ k[3];
		d += 4;
		k += 4;
	}
#endif
	memset(keystream, 0, sizeof(keystream));
}
