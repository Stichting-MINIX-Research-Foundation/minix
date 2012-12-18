/* $NetBSD: hmac.c,v 1.3 2011/05/16 10:39:12 drochner Exp $ */

/*
 * Copyright (c) 2004, Juniper Networks, Inc.
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
 * 3. Neither the name of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
/*
 * Implement HMAC as described in RFC 2104
 *
 * You need to define the following before including this file.
 *
 * HMAC_FUNC the name of the function (hmac_sha1 or hmac_md5 etc)
 * HASH_LENGTH the size of the digest (20 for SHA1, 16 for MD5)
 * HASH_CTX the name of the HASH CTX
 * HASH_Init
 * HASH_Update
 * Hash_Final
 */
#include <sys/cdefs.h>
#if !defined(lint)
__RCSID("$NetBSD: hmac.c,v 1.3 2011/05/16 10:39:12 drochner Exp $");
#endif /* not lint */

#include <stdlib.h>
#include <string.h>

/* Don't change these */
#define HMAC_IPAD 0x36
#define HMAC_OPAD 0x5c

/* Nor this */
#ifndef HMAC_BLOCKSZ
# define HMAC_BLOCKSZ 64
#endif

/*
 * The logic here is lifted straight from RFC 2104 except that
 * rather than filling the pads with 0, copying in the key and then
 * XOR with the pad byte, we just fill with the pad byte and
 * XOR with the key.
 */
void
HMAC_FUNC (const unsigned char *text, size_t text_len,
	   const unsigned char *key, size_t key_len,
	   unsigned char *digest)
{
    HASH_CTX context;
    /* Inner padding key XOR'd with ipad */
    unsigned char k_ipad[HMAC_BLOCKSZ];
    /* Outer padding key XOR'd with opad */
    unsigned char k_opad[HMAC_BLOCKSZ];
    /* HASH(key) if needed */
    unsigned char tk[HASH_LENGTH];	
    size_t i;

    /*
     * If key is longer than HMAC_BLOCKSZ bytes
     * reset it to key=HASH(key)
     */
    if (key_len > HMAC_BLOCKSZ) {
	HASH_CTX      tctx;

	HASH_Init(&tctx);
	HASH_Update(&tctx, key, key_len);
	HASH_Final(tk, &tctx);

	key = tk;
	key_len = HASH_LENGTH;
    }

    /*
     * The HMAC_ transform looks like:
     *
     * HASH(K XOR opad, HASH(K XOR ipad, text))
     *
     * where K is an n byte key
     * ipad is the byte HMAC_IPAD repeated HMAC_BLOCKSZ times
     * opad is the byte HMAC_OPAD repeated HMAC_BLOCKSZ times
     * and text is the data being protected
     */

    /*
     * Fill the pads and XOR in the key
     */
    memset( k_ipad, HMAC_IPAD, sizeof k_ipad);
    memset( k_opad, HMAC_OPAD, sizeof k_opad);
    for (i = 0; i < key_len; i++) {
	k_ipad[i] ^= key[i];
	k_opad[i] ^= key[i];
    }

    /*
     * Perform inner HASH.
     * Start with inner pad,
     * then the text.
     */
    HASH_Init(&context);
    HASH_Update(&context, k_ipad, HMAC_BLOCKSZ);
    HASH_Update(&context, text, text_len);
    HASH_Final(digest, &context);

    /*
     * Perform outer HASH.
     * Start with the outer pad,
     * then the result of the inner hash.
     */
    HASH_Init(&context);
    HASH_Update(&context, k_opad, HMAC_BLOCKSZ);
    HASH_Update(&context, digest, HASH_LENGTH);
    HASH_Final(digest, &context);
}

#if defined(MAIN) || defined(UNIT_TEST)
#include <stdio.h>


static char *
b2x(char *buf, int bufsz, unsigned char *data, int nbytes)
{
	int i;

	if (bufsz <= (nbytes * 2))
	    return NULL;
	buf[0] = '\0';
	for (i = 0; i < nbytes; i++) {
	    (void) sprintf(&buf[i*2], "%02x", data[i]);
	}
	return buf;
}

#if defined(UNIT_TEST)

static int
x2b(unsigned char *buf, int bufsz, char *data, int nbytes)
{
	int i;
	int c;

	if (nbytes < 0)
	    nbytes = strlen(data);
	nbytes /= 2;
	if (bufsz <= nbytes)
	    return 0;
	for (i = 0; i < nbytes; i++) {
	    if (sscanf(&data[i*2], "%02x", &c) < 1)
		break;
	    buf[i] = c;
	}
	buf[i] = 0;
	return i;
}

#ifndef HMAC_KAT
# define HMAC_KAT hmac_kat
#endif

/*
 * If a test key or data starts with 0x we'll convert to binary.
 */
#define X2B(v, b) do { \
    if (strncmp(v, "0x", 2) == 0) { \
        v += 2; \
        x2b(b, sizeof(b), v, strlen(v)); \
        v = b; \
    } \
} while (0)

/*
 * Run some of the known answer tests from RFC 2202
 * We assume that HASH_LENGTH==20 means SHA1 else MD5.
 */
static int
HMAC_KAT (FILE *fp)
{
    struct test_s {
	unsigned char *key;
	unsigned char *data;
	unsigned char *expect;
    } tests[] = {
	{
#if HASH_LENGTH == 20
	    "0x0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b",
	    "Hi There",
	    "0xb617318655057264e28bc0b6fb378c8ef146be00",
#else
	    "0x0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b",
	    "Hi There",
	    "0x9294727a3638bb1c13f48ef8158bfc9d",
#endif
	},
	{
	    "Jefe",
	    "what do ya want for nothing?",
#if HASH_LENGTH == 20
	    "0xeffcdf6ae5eb2fa2d27416d5f184df9c259a7c79",
#else
	    "0x750c783e6ab0b503eaa86e310a5db738",
#endif
	},
	{
#if HASH_LENGTH == 20
	    "0x0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c",
	    "Test With Truncation",
	    "0x4c1a03424b55e07fe7f27be1d58bb9324a9a5a04",
#else
	    "0x0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c",
	    "Test With Truncation",
	    "0x56461ef2342edc00f9bab995690efd4c",
#endif
	},
	{
	    "0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
	    "Test Using Larger Than Block-Size Key - Hash Key First",
#if HASH_LENGTH == 20
	    "0xaa4ae5e15272d00e95705637ce8a3b55ed402112",
#else
	    "0x6b1ab7fe4bd7bf8f0b62e6ce61b9d0cd",
#endif
	},
	{
		0, 0, 0,
	},
    };
    struct test_s *test = tests;
    unsigned char digest[HASH_LENGTH];
    unsigned char kbuf[BUFSIZ];
    unsigned char dbuf[BUFSIZ];
    unsigned char *key;
    unsigned char *data;
    char *result;
    int n = 0;
    
    for (test = tests; test->key; test++) {
	key = test->key;
	X2B(key, kbuf);
	data = test->data;
	X2B(data, dbuf);
	HMAC_FUNC(data, strlen(data), key, strlen(key), digest);
	strcpy(dbuf, "0x");
	b2x(&dbuf[2], (sizeof dbuf) - 2, digest, HASH_LENGTH);
	
	if (strcmp(dbuf, test->expect) == 0)
	    result = "Ok";
	else {
	    n++;
	    result = test->expect;
	}
	if (fp)
	    fprintf(fp, "key=%s, data=%s, result=%s: %s\n",
		    test->key, test->data, dbuf, result);
    }
    return n;
}
#endif


int
main (int argc, char *argv[])
{
    char buf[BUFSIZ];
    unsigned char *key;
    unsigned char *data;
    int key_len;
    int data_len;
    int i;
    unsigned char digest[HASH_LENGTH];

#ifdef UNIT_TEST
    if (argc == 1)
	exit(HMAC_KAT(stdout));
#endif
    
    if (argc < 3) {
	fprintf(stderr, "Usage:\n\t%s key data\n", argv[0]);
	exit(1);
    }
    key = argv[1];
    data = argv[2];
    key_len = strlen(key);
    data_len = strlen(data);
    HMAC_FUNC(data, data_len, key, key_len, digest);
    printf("0x%s\n", b2x(buf, sizeof buf, digest, HASH_LENGTH));
    exit(0);
}
#endif

		
