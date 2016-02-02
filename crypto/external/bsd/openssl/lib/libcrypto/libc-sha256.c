/*
 * Special version of sha256.c that uses the libc SHA256 implementation
 * of libc.
 */

/* crypto/sha/sha256.c */
/* ====================================================================
 * Copyright (c) 2004 The OpenSSL Project.  All rights reserved
 * according to the OpenSSL license [found in ../../LICENSE].
 * ====================================================================
 */
#include <openssl/opensslconf.h>

#include <stdlib.h>
#include <string.h>

#include <openssl/crypto.h>
#include <openssl/sha.h>
#include <openssl/opensslv.h>

#include "cryptlib.h"

const char SHA256_version[]="SHA-256" OPENSSL_VERSION_PTEXT;

unsigned char *SHA224(const unsigned char *d, size_t n, unsigned char *md)
	{
	SHA256_CTX c;
	static unsigned char m[SHA224_DIGEST_LENGTH];

	if (md == NULL) md=m;
	SHA224_Init(&c);
	SHA224_Update(&c,d,n);
	SHA224_Final(md,&c);
	OPENSSL_cleanse(&c,sizeof(c));
	return(md);
	}

unsigned char *SHA256(const unsigned char *d, size_t n, unsigned char *md)
	{
	SHA256_CTX c;
	static unsigned char m[SHA256_DIGEST_LENGTH];

	if (md == NULL) md=m;
	SHA256_Init(&c);
	SHA256_Update(&c,d,n);
	SHA256_Final(md,&c);
	OPENSSL_cleanse(&c,sizeof(c));
	return(md);
	}
