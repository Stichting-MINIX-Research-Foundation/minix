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
#ifndef STUBS_H_
#define STUBS_H_	20120327

#ifndef __BEGIN_DECLS
#  if defined(__cplusplus)
#  define __BEGIN_DECLS           extern "C" {
#  define __END_DECLS             }
#  else
#  define __BEGIN_DECLS
#  define __END_DECLS
#  endif
#endif

__BEGIN_DECLS

/*********************************/
/* stubs */
/*********************************/

void OpenSSL_add_all_algorithms(void);
void OPENSSL_add_all_algorithms_noconf(void);
void CRYPTO_cleanup_all_ex_data(void);

#include <stdio.h>

#define BIO	FILE

BIO *BIO_new_fd(int /*fd*/, int /*close_flag*/);

unsigned long ERR_get_error(void);
char *ERR_error_string(unsigned long /*e*/, char */*buf*/);
void ERR_remove_state(unsigned long /*pid*/);
void ERR_print_errors(BIO */*bp*/);

#define IDEA_KEY_SCHEDULE	void
#define des_key_schedule	void
#define des_cblock		void
#define IDEA_DECRYPT		0
#define IDEA_ENCRYPT		1
#define IDEA_BLOCK		8
#define IDEA_KEY_LENGTH		16

void idea_set_encrypt_key(uint8_t *key, IDEA_KEY_SCHEDULE *ks);
void idea_set_decrypt_key(IDEA_KEY_SCHEDULE *encrypt_ks, IDEA_KEY_SCHEDULE *decrypt_ks);
void idea_cfb64_encrypt(uint8_t *in, uint8_t *out, long length, des_key_schedule *ks, des_cblock *ivec, int *num, int enc);
void idea_ecb_encrypt(uint8_t *in, uint8_t *out, IDEA_KEY_SCHEDULE *ks);

#define CAMELLIA_KEY		void
#define CAMELLIA_DECRYPT	0
#define CAMELLIA_ENCRYPT	1
#define CAMELLIA_BLOCK_SIZE	16

int Camellia_set_key(const unsigned char *userKey, const int bits, CAMELLIA_KEY *key);
void Camellia_encrypt(const unsigned char *in, unsigned char *out, const CAMELLIA_KEY *key);
void Camellia_cfb128_encrypt(const unsigned char *in, unsigned char *out, size_t length, const CAMELLIA_KEY *key, unsigned char *ivec, int *num, const int enc);
void Camellia_decrypt(const unsigned char *in, unsigned char *out, const CAMELLIA_KEY *key);

#define const_DES_cblock	void
#define DES_cblock		void
#define DES_key_schedule	void
#define DES_DECRYPT		0
#define DES_ENCRYPT		1

int DES_set_key(const_DES_cblock *key, DES_key_schedule *schedule);
void DES_ecb3_encrypt(const_DES_cblock *input, DES_cblock *output, DES_key_schedule *ks1,DES_key_schedule *ks2, DES_key_schedule *ks3, int enc);
void DES_ede3_cfb64_encrypt(const unsigned char *in,unsigned char *out, long length,DES_key_schedule *ks1, DES_key_schedule *ks2,DES_key_schedule *ks3, DES_cblock *ivec,int *num,int enc);

__END_DECLS

#endif
