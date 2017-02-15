/*-
 * Copyright (c) 1991, 1993
 *	Dave Safford.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 */

#include <sys/cdefs.h>

#ifdef notdef
__FBSDID("$FreeBSD: src/contrib/telnet/libtelnet/pk.c,v 1.10 2002/08/22 06:19:07 nsayer Exp $");
#else
__RCSID("$NetBSD: pk.c,v 1.4 2005/02/19 22:55:35 christos Exp $");
#endif

/* public key routines */
/* functions:
	genkeys(char *public, char *secret)
	common_key(char *secret, char *public, desData *deskey)
	pk_encode(char *in, *out, DesData *deskey);
	pk_decode(char *in, *out, DesData *deskey);
      where
	char public[HEXKEYBYTES + 1];
	char secret[HEXKEYBYTES + 1];
 */

#include <sys/time.h>
#include <des.h>
#include <openssl/bn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pk.h"
 
static void adjust(char *, const char *);

/*
 * Choose top 128 bits of the common key to use as our idea key.
 */
static void
extractideakey(BIGNUM *ck, IdeaData *ideakey)
{
	BIGNUM *a = BN_new();
	BIGNUM *z = BN_new();
	BIGNUM *base = BN_new();
	BN_CTX *ctx = BN_CTX_new();
	size_t i;
	char *k;

	(void)BN_zero(a);
	(void)BN_zero(z);
	(void)BN_set_word(base, 1 << 8);
	BN_add(a, ck, z);

	for (i = 0; i < ((KEYSIZE - 128) / 8); i++)
		BN_div(a, z, a, base, ctx);

	k = (char *)(void *)ideakey;
	for (i = 0; i < 16; i++) {
		BN_div(a, z, a, base, ctx);
		*k++ = (char)BN_get_word(z);
	}

	BN_CTX_free(ctx);
	BN_free(base);
	BN_free(z);
	BN_free(a);
}

/*
 * Choose middle 64 bits of the common key to use as our des key, possibly
 * overwriting the lower order bits by setting parity. 
 */
static void
extractdeskey(BIGNUM *ck, DesData *deskey)
{
	BIGNUM *a = BN_new();
	BIGNUM *z = BN_new();
	BIGNUM *base = BN_new();
	BN_CTX *ctx = BN_CTX_new();
	size_t i;
	char *k;

	(void)BN_zero(z);
	(void)BN_zero(a);
	(void)BN_set_word(base, 1 << 8);
	BN_add(a, ck, z);

	for (i = 0; i < ((KEYSIZE - 64) / 2) / 8; i++)
		BN_div(a, z, a, base, ctx);

	k = (char *)deskey;
	for (i = 0; i < 8; i++) {
		BN_div(a, z, a, base, ctx);
		*k++ = (char)BN_get_word(z);
	}

	BN_CTX_free(ctx);
	BN_free(base);
	BN_free(z);
	BN_free(a);
}

/*
 * get common key from my secret key and his public key
 */
void
common_key(char *xsecret, char *xpublic, IdeaData *ideakey, DesData *deskey)
{
	BIGNUM *public = BN_new();
	BIGNUM *secret = BN_new();
	BIGNUM *common = BN_new();
	BIGNUM *modulus	 = BN_new();
	BN_CTX *ctx = BN_CTX_new();

	(void)BN_hex2bn(&modulus, HEXMODULUS);
	(void)BN_hex2bn(&public, xpublic);
	(void)BN_hex2bn(&secret, xsecret);
	(void)BN_zero(common);

	BN_mod_exp(common, public, secret, modulus, ctx);
	extractdeskey(common, deskey);
	extractideakey(common, ideakey);
	des_set_odd_parity(deskey);

	BN_CTX_free(ctx);
	BN_free(common);
	BN_free(secret);
	BN_free(public);
	BN_free(modulus);
}

/*
 * Generate a seed
 */
static void
getseed(char *seed, size_t seedsize)
{
	size_t i;

	for (i = 0; i < seedsize; i++)
		seed[i] = arc4random() & 0xff;
}

/*
 * Generate a random public/secret key pair
 */
void
genkeys(char *public, char *secret)
{
	size_t i;
 
#	define BASEBITS (8 * sizeof(short) - 1)
#	define BASE (1 << BASEBITS)
 
	BIGNUM *pk = BN_new();
	BIGNUM *sk = BN_new();
	BIGNUM *tmp = BN_new();
	BIGNUM *base = BN_new();
	BIGNUM *root = BN_new();
	BIGNUM *modulus = BN_new();
	BN_CTX *ctx = BN_CTX_new();
	short r;
	unsigned short seed[KEYSIZE/BASEBITS + 1];
	char *xkey;

	(void)BN_zero(pk);
	(void)BN_zero(sk);
	(void)BN_set_word(base, BASE);
	(void)BN_set_word(root, PROOT);
	(void)BN_hex2bn(&modulus, HEXMODULUS);

	getseed((char *)seed, sizeof(seed));	
	for (i = 0; i < KEYSIZE/BASEBITS + 1; i++) {
		r = seed[i] % BASE;
		(void)BN_set_word(tmp, r);
		BN_mul(sk, tmp, sk, ctx);
		BN_add(sk, tmp, sk);
	}

	(void)BN_zero(tmp);
	BN_div(tmp, sk, sk, modulus, ctx);
	BN_mod_exp(pk, root, sk, modulus, ctx);

	xkey = BN_bn2hex(sk);	
	adjust(secret, xkey);
	xkey = BN_bn2hex(pk);
	adjust(public, xkey);

	BN_CTX_free(ctx);
	BN_free(sk);
	BN_free(base);
	BN_free(pk);
	BN_free(tmp);
	BN_free(root);
	BN_free(modulus);
} 

/*
 * Adjust the input key so that it is 0-filled on the left
 */
static void
adjust(char *keyout, const char *keyin)
{
	const char *p;
	char *s;

	for (p = keyin; *p; p++) 
		continue;
	for (s = keyout + HEXKEYBYTES; p >= keyin; p--, s--)
		*s = *p;
	while (s >= keyout)
		*s-- = '0';
}

static const char hextab[] = "0123456789ABCDEF";

/* given a DES key, cbc encrypt and translate input to terminated hex */
void
pk_encode(const char *in, char *out, DesData *key)
{
	char buf[256];
	DesData i;
	des_key_schedule k;
	size_t l, op, deslen;

	(void)memset(&i, 0, sizeof(i));
	(void)memset(buf, 0, sizeof(buf));
	deslen = ((strlen(in) + 7) / 8) * 8;
	des_key_sched(key, k);
	des_cbc_encrypt(in, buf, deslen, k, &i, DES_ENCRYPT);
	for (l = 0, op = 0; l < deslen; l++) {
		out[op++] = hextab[(buf[l] & 0xf0) >> 4];
		out[op++] = hextab[(buf[l] & 0x0f)];
	}
	out[op] = '\0';
}

/* given a DES key, translate input from hex and decrypt */
void
pk_decode(const char *in, char *out, DesData *key)
{
	char buf[256];
	DesData i;
	des_key_schedule k;
	int n1, n2, op;
	size_t l;
	size_t len = strlen(in) / 2;

	(void)memset(&i, 0, sizeof(i));
	(void)memset(buf, 0, sizeof(buf));

	for (l = 0, op = 0; l < len; l++, op += 2) {
		if (in[op] > '9')
			n1 = in[op] - 'A' + 10;
		else
			n1 = in[op] - '0';
		if (in[op + 1] > '9')
			n2 = in[op + 1] - 'A' + 10;
		else
			n2 = in[op + 1] - '0';
		buf[l] = (char)(n1 * 16 + n2);
	}
	des_key_sched(key, k);
	des_cbc_encrypt(buf, out, len, k, &i, DES_DECRYPT);
	out[len] = '\0';
}
