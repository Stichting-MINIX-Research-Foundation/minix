/*	$NetBSD: bcrypt.c,v 1.17 2012/08/30 12:16:49 drochner Exp $	*/
/*	$OpenBSD: bcrypt.c,v 1.16 2002/02/19 19:39:36 millert Exp $	*/

/*
 * Copyright 1997 Niels Provos <provos@physnet.uni-hamburg.de>
 * All rights reserved.
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
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* This password hashing algorithm was designed by David Mazieres
 * <dm@lcs.mit.edu> and works as follows:
 *
 * 1. state := InitState ()
 * 2. state := ExpandKey (state, salt, password) 3.
 * REPEAT rounds:
 *	state := ExpandKey (state, 0, salt)
 *      state := ExpandKey(state, 0, password)
 * 4. ctext := "OrpheanBeholderScryDoubt"
 * 5. REPEAT 64:
 * 	ctext := Encrypt_ECB (state, ctext);
 * 6. RETURN Concatenate (salt, ctext);
 *
 */
#include <sys/cdefs.h>
__RCSID("$NetBSD: bcrypt.c,v 1.17 2012/08/30 12:16:49 drochner Exp $");

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <limits.h>

#include "crypt.h"
#include "blowfish.c"

/* This implementation is adaptable to current computing power.
 * You can have up to 2^31 rounds which should be enough for some
 * time to come.
 */

#define BCRYPT_VERSION '2'
#define BCRYPT_MAXSALT 16	/* Precomputation is just so nice */
#define BCRYPT_MAXSALTLEN 	(7 + (BCRYPT_MAXSALT * 4 + 2) / 3 + 1)
#define BCRYPT_BLOCKS 6		/* Ciphertext blocks */
#define BCRYPT_MINROUNDS 16	/* we have log2(rounds) in salt */

static void encode_salt(char *, u_int8_t *, u_int16_t, u_int8_t);
static void encode_base64(u_int8_t *, u_int8_t *, u_int16_t);
static void decode_base64(u_int8_t *, u_int16_t, const u_int8_t *);

char *__bcrypt(const char *, const char *);	/* XXX */

static char    encrypted[_PASSWORD_LEN];

static const u_int8_t Base64Code[] =
"./ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

char *bcrypt_gensalt(u_int8_t);

static const u_int8_t index_64[128] =
{
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 0, 1, 54, 55,
	56, 57, 58, 59, 60, 61, 62, 63, 255, 255,
	255, 255, 255, 255, 255, 2, 3, 4, 5, 6,
	7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
	17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,
	255, 255, 255, 255, 255, 255, 28, 29, 30,
	31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
	51, 52, 53, 255, 255, 255, 255, 255
};
#define CHAR64(c)  ( (c) > 127 ? 255 : index_64[(c)])

static void
decode_base64(u_int8_t *buffer, u_int16_t len, const u_int8_t *data)
{
	u_int8_t *bp = buffer;
	const u_int8_t *p = data;
	u_int8_t c1, c2, c3, c4;
	while (bp < buffer + len) {
		c1 = CHAR64(*p);
		c2 = CHAR64(*(p + 1));

		/* Invalid data */
		if (c1 == 255 || c2 == 255)
			break;

		*bp++ = ((u_int32_t)c1 << 2) | (((u_int32_t)c2 & 0x30) >> 4);
		if (bp >= buffer + len)
			break;

		c3 = CHAR64(*(p + 2));
		if (c3 == 255)
			break;

		*bp++ = (((u_int32_t)c2 & 0x0f) << 4) | (((uint32_t)c3 & 0x3c) >> 2);
		if (bp >= buffer + len)
			break;

		c4 = CHAR64(*(p + 3));
		if (c4 == 255)
			break;
		*bp++ = ((c3 & 0x03) << 6) | c4;

		p += 4;
	}
}

static void
encode_salt(char *salt, u_int8_t *csalt, u_int16_t clen, u_int8_t logr)
{
	salt[0] = '$';
	salt[1] = BCRYPT_VERSION;
	salt[2] = 'a';
	salt[3] = '$';

	snprintf(salt + 4, 4, "%2.2u$", logr);

	encode_base64((u_int8_t *) salt + 7, csalt, clen);
}

int
__gensalt_blowfish(char *salt, size_t saltlen, const char *option)
{
	size_t i;
	u_int32_t seed = 0;
	u_int8_t csalt[BCRYPT_MAXSALT];
	unsigned long nrounds;
	char *ep;

	if (saltlen < BCRYPT_MAXSALTLEN) {
		errno = ENOSPC;
		return -1;
	}
	if (option == NULL) {
		errno = EINVAL;
		return -1;
	}
	nrounds = strtoul(option, &ep, 0);
	if (option == ep || *ep) {
		errno = EINVAL;
		return -1;
	}
	if (errno == ERANGE && nrounds == ULONG_MAX)
		return -1;

	if (nrounds < 4)
		nrounds = 4;
	else if (nrounds > 31)
		nrounds = 31;

	for (i = 0; i < BCRYPT_MAXSALT; i++) {
		if (i % 4 == 0)
			seed = arc4random();
		csalt[i] = seed & 0xff;
		seed = seed >> 8;
	}
	encode_salt(salt, csalt, BCRYPT_MAXSALT, (u_int8_t)nrounds);
	return 0;
}

/* Generates a salt for this version of crypt.
   Since versions may change. Keeping this here
   seems sensible.
   XXX: compat.
 */
char *
bcrypt_gensalt(u_int8_t log_rounds)
{
	static char gsalt[BCRYPT_MAXSALTLEN];
	char num[10];

	(void)snprintf(num, sizeof(num), "%d", log_rounds);
	if (__gensalt_blowfish(gsalt, sizeof(gsalt), num) == -1)
		return NULL;
	return gsalt;
}

/* We handle $Vers$log2(NumRounds)$salt+passwd$
   i.e. $2$04$iwouldntknowwhattosayetKdJ6iFtacBqJdKe6aW7ou */

char   *
__bcrypt(const char *key, const char *salt)
{
	blf_ctx state;
	u_int32_t rounds, i, k;
	u_int16_t j;
	u_int8_t key_len, salt_len, logr, minor;
	u_int8_t ciphertext[4 * BCRYPT_BLOCKS] = "OrpheanBeholderScryDoubt";
	u_int8_t csalt[BCRYPT_MAXSALT];
	u_int32_t cdata[BCRYPT_BLOCKS];
	int n;
	size_t len;

	/* Discard "$" identifier */
	salt++;

	if (*salt > BCRYPT_VERSION)
		return NULL;

	/* Check for minor versions */
	if (salt[1] != '$') {
		switch (salt[1]) {
		case 'a':
			/* 'ab' should not yield the same as 'abab' */
			minor = salt[1];
			salt++;
			break;
		default:
			return NULL;
		}
	} else
		 minor = 0;

	/* Discard version + "$" identifier */
	salt += 2;

	if (salt[2] != '$')
		/* Out of sync with passwd entry */
		return NULL;

	/* Computer power doesn't increase linear, 2^x should be fine */
	n = atoi(salt);
	if (n > 31 || n < 0)
		return NULL;
	logr = (u_int8_t)n;
	if ((rounds = (u_int32_t) 1 << logr) < BCRYPT_MINROUNDS)
		return NULL;

	/* Discard num rounds + "$" identifier */
	salt += 3;

	if (strlen(salt) * 3 / 4 < BCRYPT_MAXSALT)
		return NULL;

	/* We dont want the base64 salt but the raw data */
	decode_base64(csalt, BCRYPT_MAXSALT, (const u_int8_t *)salt);
	salt_len = BCRYPT_MAXSALT;
	len = strlen(key);
	if (len > 72)
		key_len = 72;
	else
		key_len = (uint8_t)len;
	key_len += minor >= 'a' ? 1 : 0;

	/* Setting up S-Boxes and Subkeys */
	Blowfish_initstate(&state);
	Blowfish_expandstate(&state, csalt, salt_len,
	    (const u_int8_t *) key, key_len);
	for (k = 0; k < rounds; k++) {
		Blowfish_expand0state(&state, (const u_int8_t *) key, key_len);
		Blowfish_expand0state(&state, csalt, salt_len);
	}

	/* This can be precomputed later */
	j = 0;
	for (i = 0; i < BCRYPT_BLOCKS; i++)
		cdata[i] = Blowfish_stream2word(ciphertext, 4 * BCRYPT_BLOCKS, &j);

	/* Now do the encryption */
	for (k = 0; k < 64; k++)
		blf_enc(&state, cdata, BCRYPT_BLOCKS / 2);

	for (i = 0; i < BCRYPT_BLOCKS; i++) {
		ciphertext[4 * i + 3] = cdata[i] & 0xff;
		cdata[i] = cdata[i] >> 8;
		ciphertext[4 * i + 2] = cdata[i] & 0xff;
		cdata[i] = cdata[i] >> 8;
		ciphertext[4 * i + 1] = cdata[i] & 0xff;
		cdata[i] = cdata[i] >> 8;
		ciphertext[4 * i + 0] = cdata[i] & 0xff;
	}


	i = 0;
	encrypted[i++] = '$';
	encrypted[i++] = BCRYPT_VERSION;
	if (minor)
		encrypted[i++] = minor;
	encrypted[i++] = '$';

	snprintf(encrypted + i, 4, "%2.2u$", logr);

	encode_base64((u_int8_t *) encrypted + i + 3, csalt, BCRYPT_MAXSALT);
	encode_base64((u_int8_t *) encrypted + strlen(encrypted), ciphertext,
	    4 * BCRYPT_BLOCKS - 1);
	__explicit_bzero(&state, sizeof(state));
	return encrypted;
}

static void
encode_base64(u_int8_t *buffer, u_int8_t *data, u_int16_t len)
{
	u_int8_t *bp = buffer;
	u_int8_t *p = data;
	u_int8_t c1, c2;
	while (p < data + len) {
		c1 = *p++;
		*bp++ = Base64Code[((u_int32_t)c1 >> 2)];
		c1 = (c1 & 0x03) << 4;
		if (p >= data + len) {
			*bp++ = Base64Code[c1];
			break;
		}
		c2 = *p++;
		c1 |= ((u_int32_t)c2 >> 4) & 0x0f;
		*bp++ = Base64Code[c1];
		c1 = (c2 & 0x0f) << 2;
		if (p >= data + len) {
			*bp++ = Base64Code[c1];
			break;
		}
		c2 = *p++;
		c1 |= ((u_int32_t)c2 >> 6) & 0x03;
		*bp++ = Base64Code[c1];
		*bp++ = Base64Code[c2 & 0x3f];
	}
	*bp = '\0';
}
#if 0
void
main()
{
	char    blubber[73];
	char    salt[100];
	char   *p;
	salt[0] = '$';
	salt[1] = BCRYPT_VERSION;
	salt[2] = '$';

	snprintf(salt + 3, 4, "%2.2u$", 5);

	printf("24 bytes of salt: ");
	fgets(salt + 6, 94, stdin);
	salt[99] = 0;
	printf("72 bytes of password: ");
	fpurge(stdin);
	fgets(blubber, 73, stdin);
	blubber[72] = 0;

	p = crypt(blubber, salt);
	printf("Passwd entry: %s\n\n", p);

	p = bcrypt_gensalt(5);
	printf("Generated salt: %s\n", p);
	p = crypt(blubber, p);
	printf("Passwd entry: %s\n", p);
}
#endif
