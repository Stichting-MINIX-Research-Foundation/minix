/*-
 * Copyright (c) 2012 Alistair Crooks <agc@NetBSD.org>
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
#include <sys/types.h>
#include <sys/param.h>

#ifdef _KERNEL
# include <sys/kmem.h>
#else
# include <arpa/inet.h>
# include <ctype.h>
# include <inttypes.h>
# include <stdarg.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <time.h>
# include <unistd.h>
#endif

#include "misc.h"
#include "bn.h"

#include "stubs.h"

#ifndef USE_ARG
#define USE_ARG(x)	/*LINTED*/(void)&(x)
#endif

void
OpenSSL_add_all_algorithms(void)
{
}

void
OPENSSL_add_all_algorithms_noconf(void)
{
}

void
CRYPTO_cleanup_all_ex_data(void)
{
}

BIO *
BIO_new_fd(int fd, int close_flag)
{
	USE_ARG(fd);
	USE_ARG(close_flag);
	return NULL;
}

unsigned long
ERR_get_error(void)
{
	return 0;
}

char *
ERR_error_string(unsigned long e, char *buf)
{
	static char	staticbuf[120];

	if (buf == NULL) {
		buf = staticbuf;
	}
	snprintf(buf, 120, "error:%lu:netssl:[unknown function]:[unknown error]", e);
	return buf;
}

void
ERR_remove_state(unsigned long pid)
{
	USE_ARG(pid);
}

void
ERR_print_errors(BIO *bp)
{
	USE_ARG(bp);
}

void
idea_set_encrypt_key(uint8_t *key, IDEA_KEY_SCHEDULE *ks)
{
	printf("idea_set_encrypt_key stubbed\n");
	USE_ARG(key);
	USE_ARG(ks);
}

void
idea_set_decrypt_key(IDEA_KEY_SCHEDULE *encrypt_ks, IDEA_KEY_SCHEDULE *decrypt_ks)
{
	printf("idea_set_decrypt_key stubbed\n");
	USE_ARG(encrypt_ks);
	USE_ARG(decrypt_ks);
}

void
idea_cfb64_encrypt(uint8_t *in, uint8_t *out, long length, des_key_schedule *ks, des_cblock *ivec, int *num, int enc)
{
	printf("idea_cfb64_encrypt stubbed\n");
	USE_ARG(in);
	USE_ARG(out);
	USE_ARG(length);
	USE_ARG(ks);
	USE_ARG(ivec);
	USE_ARG(num);
	USE_ARG(enc);
}

void
idea_ecb_encrypt(uint8_t *in, uint8_t *out, IDEA_KEY_SCHEDULE *ks)
{
	printf("idea_cfb64_decrypt stubbed\n");
	USE_ARG(in);
	USE_ARG(out);
	USE_ARG(ks);
}

int
Camellia_set_key(const unsigned char *userKey, const int bits, CAMELLIA_KEY *key)
{
	printf("Camellia_set_key stubbed\n");
	USE_ARG(userKey);
	USE_ARG(bits);
	USE_ARG(key);
	return 0;
}

void
Camellia_encrypt(const unsigned char *in, unsigned char *out, const CAMELLIA_KEY *key)
{
	printf("Camellia_encrypt stubbed\n");
	USE_ARG(in);
	USE_ARG(out);
	USE_ARG(key);
}

void
Camellia_cfb128_encrypt(const unsigned char *in, unsigned char *out, size_t length, const CAMELLIA_KEY *key, unsigned char *ivec, int *num, const int enc)
{
	printf("Camellia_cfb128_encrypt stubbed\n");
	USE_ARG(in);
	USE_ARG(out);
	USE_ARG(length);
	USE_ARG(key);
	USE_ARG(ivec);
	USE_ARG(num);
	USE_ARG(enc);
}

void
Camellia_decrypt(const unsigned char *in, unsigned char *out, const CAMELLIA_KEY *key)
{
	printf("Camellia_decrypt stubbed\n");
	USE_ARG(in);
	USE_ARG(out);
	USE_ARG(key);
}

int
DES_set_key(const_DES_cblock *key, DES_key_schedule *schedule)
{
	printf("DES_set_key stubbed\n");
	USE_ARG(key);
	USE_ARG(schedule);
	return 0;
}

void
DES_ecb3_encrypt(const_DES_cblock *input, DES_cblock *output, DES_key_schedule *ks1,DES_key_schedule *ks2, DES_key_schedule *ks3, int enc)
{
	printf("DES_ecb3_encrypt stubbed\n");
	USE_ARG(input);
	USE_ARG(output);
	USE_ARG(ks1);
	USE_ARG(ks2);
	USE_ARG(ks3);
	USE_ARG(enc);
}

void
DES_ede3_cfb64_encrypt(const unsigned char *in,unsigned char *out, long length,DES_key_schedule *ks1, DES_key_schedule *ks2,DES_key_schedule *ks3, DES_cblock *ivec,int *num,int enc)
{
	printf("DES_ede3_cfb64_encrypt stubbed\n");
	USE_ARG(in);
	USE_ARG(out);
	USE_ARG(length);
	USE_ARG(ks1);
	USE_ARG(ks2);
	USE_ARG(ks3);
	USE_ARG(ivec);
	USE_ARG(num);
	USE_ARG(enc);
}
