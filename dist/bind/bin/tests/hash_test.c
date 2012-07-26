/*
 * Copyright (C) 2004-2007  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000, 2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: hash_test.c,v 1.19 2007-06-19 23:46:59 tbox Exp $ */

/*! \file */
#include <config.h>

#include <stdio.h>
#include <string.h>

#include <isc/hmacmd5.h>
#include <isc/hmacsha.h>
#include <isc/md5.h>
#include <isc/sha1.h>
#include <isc/util.h>
#include <isc/string.h>

static void
print_digest(const char *s, const char *hash, unsigned char *d,
	     unsigned int words)
{
	unsigned int i, j;

	printf("hash (%s) %s:\n\t", hash, s);
	for (i = 0; i < words; i++) {
		printf(" ");
		for (j = 0; j < 4; j++)
			printf("%02x", d[i * 4 + j]);
	}
	printf("\n");
}

int
main(int argc, char **argv) {
	isc_sha1_t sha1;
	isc_sha224_t sha224;
	isc_md5_t md5;
	isc_hmacmd5_t hmacmd5;
	isc_hmacsha1_t hmacsha1;
	isc_hmacsha224_t hmacsha224;
	isc_hmacsha256_t hmacsha256;
	isc_hmacsha384_t hmacsha384;
	isc_hmacsha512_t hmacsha512;
	unsigned char digest[ISC_SHA512_DIGESTLENGTH];
	unsigned char buffer[1024];
	const char *s;
	unsigned char key[20];

	UNUSED(argc);
	UNUSED(argv);

	s = "abc";
	isc_sha1_init(&sha1);
	memcpy(buffer, s, strlen(s));
	isc_sha1_update(&sha1, buffer, strlen(s));
	isc_sha1_final(&sha1, digest);
	print_digest(s, "sha1", digest, ISC_SHA1_DIGESTLENGTH/4);

	s = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
	isc_sha1_init(&sha1);
	memcpy(buffer, s, strlen(s));
	isc_sha1_update(&sha1, buffer, strlen(s));
	isc_sha1_final(&sha1, digest);
	print_digest(s, "sha1", digest, ISC_SHA1_DIGESTLENGTH/4);

	s = "abc";
	isc_sha224_init(&sha224);
	memcpy(buffer, s, strlen(s));
	isc_sha224_update(&sha224, buffer, strlen(s));
	isc_sha224_final(digest, &sha224);
	print_digest(s, "sha224", digest, ISC_SHA224_DIGESTLENGTH/4);

	s = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
	isc_sha224_init(&sha224);
	memcpy(buffer, s, strlen(s));
	isc_sha224_update(&sha224, buffer, strlen(s));
	isc_sha224_final(digest, &sha224);
	print_digest(s, "sha224", digest, ISC_SHA224_DIGESTLENGTH/4);

	s = "abc";
	isc_md5_init(&md5);
	memcpy(buffer, s, strlen(s));
	isc_md5_update(&md5, buffer, strlen(s));
	isc_md5_final(&md5, digest);
	print_digest(s, "md5", digest, 4);

	/*
	 * The 3 HMAC-MD5 examples from RFC2104
	 */
	s = "Hi There";
	memset(key, 0x0b, 16);
	isc_hmacmd5_init(&hmacmd5, key, 16);
	memcpy(buffer, s, strlen(s));
	isc_hmacmd5_update(&hmacmd5, buffer, strlen(s));
	isc_hmacmd5_sign(&hmacmd5, digest);
	print_digest(s, "hmacmd5", digest, 4);

	s = "what do ya want for nothing?";
	strcpy((char *)key, "Jefe");
	isc_hmacmd5_init(&hmacmd5, key, 4);
	memcpy(buffer, s, strlen(s));
	isc_hmacmd5_update(&hmacmd5, buffer, strlen(s));
	isc_hmacmd5_sign(&hmacmd5, digest);
	print_digest(s, "hmacmd5", digest, 4);

	s = "\335\335\335\335\335\335\335\335\335\335"
	    "\335\335\335\335\335\335\335\335\335\335"
	    "\335\335\335\335\335\335\335\335\335\335"
	    "\335\335\335\335\335\335\335\335\335\335"
	    "\335\335\335\335\335\335\335\335\335\335";
	memset(key, 0xaa, 16);
	isc_hmacmd5_init(&hmacmd5, key, 16);
	memcpy(buffer, s, strlen(s));
	isc_hmacmd5_update(&hmacmd5, buffer, strlen(s));
	isc_hmacmd5_sign(&hmacmd5, digest);
	print_digest(s, "hmacmd5", digest, 4);

	/*
	 * The 3 HMAC-SHA1 examples from RFC4634.
	 */
	s = "Hi There";
	memset(key, 0x0b, 20);
	isc_hmacsha1_init(&hmacsha1, key, 20);
	memcpy(buffer, s, strlen(s));
	isc_hmacsha1_update(&hmacsha1, buffer, strlen(s));
	isc_hmacsha1_sign(&hmacsha1, digest, ISC_SHA1_DIGESTLENGTH);
	print_digest(s, "hmacsha1", digest, ISC_SHA1_DIGESTLENGTH/4);

	s = "what do ya want for nothing?";
	strcpy((char *)key, "Jefe");
	isc_hmacsha1_init(&hmacsha1, key, 4);
	memcpy(buffer, s, strlen(s));
	isc_hmacsha1_update(&hmacsha1, buffer, strlen(s));
	isc_hmacsha1_sign(&hmacsha1, digest, ISC_SHA1_DIGESTLENGTH);
	print_digest(s, "hmacsha1", digest, ISC_SHA1_DIGESTLENGTH/4);

	s = "\335\335\335\335\335\335\335\335\335\335"
	    "\335\335\335\335\335\335\335\335\335\335"
	    "\335\335\335\335\335\335\335\335\335\335"
	    "\335\335\335\335\335\335\335\335\335\335"
	    "\335\335\335\335\335\335\335\335\335\335";
	memset(key, 0xaa, 20);
	isc_hmacsha1_init(&hmacsha1, key, 20);
	memcpy(buffer, s, strlen(s));
	isc_hmacsha1_update(&hmacsha1, buffer, strlen(s));
	isc_hmacsha1_sign(&hmacsha1, digest, ISC_SHA1_DIGESTLENGTH);
	print_digest(s, "hmacsha1", digest, ISC_SHA1_DIGESTLENGTH/4);

	/*
	 * The 3 HMAC-SHA224 examples from RFC4634.
	 */
	s = "Hi There";
	memset(key, 0x0b, 20);
	isc_hmacsha224_init(&hmacsha224, key, 20);
	memcpy(buffer, s, strlen(s));
	isc_hmacsha224_update(&hmacsha224, buffer, strlen(s));
	isc_hmacsha224_sign(&hmacsha224, digest, ISC_SHA224_DIGESTLENGTH);
	print_digest(s, "hmacsha224", digest, ISC_SHA224_DIGESTLENGTH/4);

	s = "what do ya want for nothing?";
	strcpy((char *)key, "Jefe");
	isc_hmacsha224_init(&hmacsha224, key, 4);
	memcpy(buffer, s, strlen(s));
	isc_hmacsha224_update(&hmacsha224, buffer, strlen(s));
	isc_hmacsha224_sign(&hmacsha224, digest, ISC_SHA224_DIGESTLENGTH);
	print_digest(s, "hmacsha224", digest, ISC_SHA224_DIGESTLENGTH/4);

	s = "\335\335\335\335\335\335\335\335\335\335"
	    "\335\335\335\335\335\335\335\335\335\335"
	    "\335\335\335\335\335\335\335\335\335\335"
	    "\335\335\335\335\335\335\335\335\335\335"
	    "\335\335\335\335\335\335\335\335\335\335";
	memset(key, 0xaa, 20);
	isc_hmacsha224_init(&hmacsha224, key, 20);
	memcpy(buffer, s, strlen(s));
	isc_hmacsha224_update(&hmacsha224, buffer, strlen(s));
	isc_hmacsha224_sign(&hmacsha224, digest, ISC_SHA224_DIGESTLENGTH);
	print_digest(s, "hmacsha224", digest, ISC_SHA224_DIGESTLENGTH/4);

	/*
	 * The 3 HMAC-SHA256 examples from RFC4634.
	 */
	s = "Hi There";
	memset(key, 0x0b, 20);
	isc_hmacsha256_init(&hmacsha256, key, 20);
	memcpy(buffer, s, strlen(s));
	isc_hmacsha256_update(&hmacsha256, buffer, strlen(s));
	isc_hmacsha256_sign(&hmacsha256, digest, ISC_SHA256_DIGESTLENGTH);
	print_digest(s, "hmacsha256", digest, ISC_SHA256_DIGESTLENGTH/4);

	s = "what do ya want for nothing?";
	strcpy((char *)key, "Jefe");
	isc_hmacsha256_init(&hmacsha256, key, 4);
	memcpy(buffer, s, strlen(s));
	isc_hmacsha256_update(&hmacsha256, buffer, strlen(s));
	isc_hmacsha256_sign(&hmacsha256, digest, ISC_SHA256_DIGESTLENGTH);
	print_digest(s, "hmacsha256", digest, ISC_SHA256_DIGESTLENGTH/4);

	s = "\335\335\335\335\335\335\335\335\335\335"
	    "\335\335\335\335\335\335\335\335\335\335"
	    "\335\335\335\335\335\335\335\335\335\335"
	    "\335\335\335\335\335\335\335\335\335\335"
	    "\335\335\335\335\335\335\335\335\335\335";
	memset(key, 0xaa, 20);
	isc_hmacsha256_init(&hmacsha256, key, 20);
	memcpy(buffer, s, strlen(s));
	isc_hmacsha256_update(&hmacsha256, buffer, strlen(s));
	isc_hmacsha256_sign(&hmacsha256, digest, ISC_SHA256_DIGESTLENGTH);
	print_digest(s, "hmacsha256", digest, ISC_SHA256_DIGESTLENGTH/4);

	/*
	 * The 3 HMAC-SHA384 examples from RFC4634.
	 */
	s = "Hi There";
	memset(key, 0x0b, 20);
	isc_hmacsha384_init(&hmacsha384, key, 20);
	memcpy(buffer, s, strlen(s));
	isc_hmacsha384_update(&hmacsha384, buffer, strlen(s));
	isc_hmacsha384_sign(&hmacsha384, digest, ISC_SHA384_DIGESTLENGTH);
	print_digest(s, "hmacsha384", digest, ISC_SHA384_DIGESTLENGTH/4);

	s = "what do ya want for nothing?";
	strcpy((char *)key, "Jefe");
	isc_hmacsha384_init(&hmacsha384, key, 4);
	memcpy(buffer, s, strlen(s));
	isc_hmacsha384_update(&hmacsha384, buffer, strlen(s));
	isc_hmacsha384_sign(&hmacsha384, digest, ISC_SHA384_DIGESTLENGTH);
	print_digest(s, "hmacsha384", digest, ISC_SHA384_DIGESTLENGTH/4);

	s = "\335\335\335\335\335\335\335\335\335\335"
	    "\335\335\335\335\335\335\335\335\335\335"
	    "\335\335\335\335\335\335\335\335\335\335"
	    "\335\335\335\335\335\335\335\335\335\335"
	    "\335\335\335\335\335\335\335\335\335\335";
	memset(key, 0xaa, 20);
	isc_hmacsha384_init(&hmacsha384, key, 20);
	memcpy(buffer, s, strlen(s));
	isc_hmacsha384_update(&hmacsha384, buffer, strlen(s));
	isc_hmacsha384_sign(&hmacsha384, digest, ISC_SHA384_DIGESTLENGTH);
	print_digest(s, "hmacsha384", digest, ISC_SHA384_DIGESTLENGTH/4);

	/*
	 * The 3 HMAC-SHA512 examples from RFC4634.
	 */
	s = "Hi There";
	memset(key, 0x0b, 20);
	isc_hmacsha512_init(&hmacsha512, key, 20);
	memcpy(buffer, s, strlen(s));
	isc_hmacsha512_update(&hmacsha512, buffer, strlen(s));
	isc_hmacsha512_sign(&hmacsha512, digest, ISC_SHA512_DIGESTLENGTH);
	print_digest(s, "hmacsha512", digest, ISC_SHA512_DIGESTLENGTH/4);

	s = "what do ya want for nothing?";
	strcpy((char *)key, "Jefe");
	isc_hmacsha512_init(&hmacsha512, key, 4);
	memcpy(buffer, s, strlen(s));
	isc_hmacsha512_update(&hmacsha512, buffer, strlen(s));
	isc_hmacsha512_sign(&hmacsha512, digest, ISC_SHA512_DIGESTLENGTH);
	print_digest(s, "hmacsha512", digest, ISC_SHA512_DIGESTLENGTH/4);

	s = "\335\335\335\335\335\335\335\335\335\335"
	    "\335\335\335\335\335\335\335\335\335\335"
	    "\335\335\335\335\335\335\335\335\335\335"
	    "\335\335\335\335\335\335\335\335\335\335"
	    "\335\335\335\335\335\335\335\335\335\335";
	memset(key, 0xaa, 20);
	isc_hmacsha512_init(&hmacsha512, key, 20);
	memcpy(buffer, s, strlen(s));
	isc_hmacsha512_update(&hmacsha512, buffer, strlen(s));
	isc_hmacsha512_sign(&hmacsha512, digest, ISC_SHA512_DIGESTLENGTH);
	print_digest(s, "hmacsha512", digest, ISC_SHA512_DIGESTLENGTH/4);

	return (0);
}
