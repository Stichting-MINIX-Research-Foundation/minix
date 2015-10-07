/*
 * Special version of sha512.c that uses the libc SHA512 implementation
 * of libc.
 */

/* crypto/sha/sha512.c */
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

const char SHA512_version[]="SHA-512" OPENSSL_VERSION_PTEXT;

unsigned char *SHA384(const unsigned char *d, size_t n, unsigned char *md)
	{
	SHA512_CTX c;
	static unsigned char m[SHA384_DIGEST_LENGTH];

	if (md == NULL) md=m;
	SHA384_Init(&c);
	SHA384_Update(&c,d,n);
	SHA384_Final(md,&c);
	OPENSSL_cleanse(&c,sizeof(c));
	return(md);
	}

unsigned char *SHA512(const unsigned char *d, size_t n, unsigned char *md)
	{
	SHA512_CTX c;
	static unsigned char m[SHA512_DIGEST_LENGTH];

	if (md == NULL) md=m;
	SHA512_Init(&c);
	SHA512_Update(&c,d,n);
	SHA512_Final(md,&c);
	OPENSSL_cleanse(&c,sizeof(c));
	return(md);
	}
