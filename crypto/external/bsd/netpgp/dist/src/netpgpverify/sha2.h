/*	$NetBSD: sha2.h,v 1.2 2016/06/14 20:47:08 agc Exp $	*/
/*	$KAME: sha2.h,v 1.4 2003/07/20 00:28:38 itojun Exp $	*/

/*
 * sha2.h
 *
 * Version 1.0.0beta1
 *
 * Written by Aaron D. Gifford <me@aarongifford.com>
 *
 * Copyright 2000 Aaron D. Gifford.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) AND CONTRIBUTOR(S) ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) OR CONTRIBUTOR(S) BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef __SHA2_H__
#define __SHA2_H__

#include <sys/types.h>

/*** SHA-224/256/384/512 Various Length Definitions ***********************/
#define SHA224_BLOCK_LENGTH		64
#define SHA224_DIGEST_LENGTH		28
#define SHA224_DIGEST_STRING_LENGTH	(SHA224_DIGEST_LENGTH * 2 + 1)
#define SHA256_BLOCK_LENGTH		64
#define SHA256_DIGEST_LENGTH		32
#define SHA256_DIGEST_STRING_LENGTH	(SHA256_DIGEST_LENGTH * 2 + 1)
#define SHA384_BLOCK_LENGTH		128
#define SHA384_DIGEST_LENGTH		48
#define SHA384_DIGEST_STRING_LENGTH	(SHA384_DIGEST_LENGTH * 2 + 1)
#define SHA512_BLOCK_LENGTH		128
#define SHA512_DIGEST_LENGTH		64
#define SHA512_DIGEST_STRING_LENGTH	(SHA512_DIGEST_LENGTH * 2 + 1)

#ifndef __BEGIN_DECLS
#  if defined(__cplusplus)
#  define __BEGIN_DECLS           extern "C" {
#  define __END_DECLS             }
#  else
#  define __BEGIN_DECLS
#  define __END_DECLS
#  endif
#endif

/*** SHA-256/384/512 Context Structures *******************************/
typedef struct _NETPGPV_SHA256_CTX {
	uint32_t	state[8];
	uint64_t	bitcount;
	uint8_t	buffer[SHA256_BLOCK_LENGTH];
} NETPGPV_SHA256_CTX;

typedef struct _NETPGPV_SHA512_CTX {
	uint64_t	state[8];
	uint64_t	bitcount[2];
	uint8_t	buffer[SHA512_BLOCK_LENGTH];
} NETPGPV_SHA512_CTX;

typedef NETPGPV_SHA256_CTX NETPGPV_SHA224_CTX;
typedef NETPGPV_SHA512_CTX NETPGPV_SHA384_CTX;


/*** SHA-256/384/512 Function Prototypes ******************************/
__BEGIN_DECLS
int netpgpv_SHA224_Init(NETPGPV_SHA224_CTX *);
int netpgpv_SHA224_Update(NETPGPV_SHA224_CTX*, const uint8_t*, size_t);
int netpgpv_SHA224_Final(uint8_t[SHA224_DIGEST_LENGTH], NETPGPV_SHA224_CTX*);
#ifndef _KERNEL
char *netpgpv_SHA224_End(NETPGPV_SHA224_CTX *, char[SHA224_DIGEST_STRING_LENGTH]);
char *netpgpv_SHA224_FileChunk(const char *, char *, off_t, off_t);
char *netpgpv_SHA224_File(const char *, char *);
char *netpgpv_SHA224_Data(const uint8_t *, size_t, char[SHA224_DIGEST_STRING_LENGTH]);
#endif /* !_KERNEL */

int netpgpv_SHA256_Init(NETPGPV_SHA256_CTX *);
int netpgpv_SHA256_Update(NETPGPV_SHA256_CTX*, const uint8_t*, size_t);
int netpgpv_SHA256_Final(uint8_t[SHA256_DIGEST_LENGTH], NETPGPV_SHA256_CTX*);
#ifndef _KERNEL
char *netpgpv_SHA256_End(NETPGPV_SHA256_CTX *, char[SHA256_DIGEST_STRING_LENGTH]);
char *netpgpv_SHA256_FileChunk(const char *, char *, off_t, off_t);
char *netpgpv_SHA256_File(const char *, char *);
char *netpgpv_SHA256_Data(const uint8_t *, size_t, char[SHA256_DIGEST_STRING_LENGTH]);
#endif /* !_KERNEL */

int netpgpv_SHA384_Init(NETPGPV_SHA384_CTX*);
int netpgpv_SHA384_Update(NETPGPV_SHA384_CTX*, const uint8_t*, size_t);
int netpgpv_SHA384_Final(uint8_t[SHA384_DIGEST_LENGTH], NETPGPV_SHA384_CTX*);
#ifndef _KERNEL
char *netpgpv_SHA384_End(NETPGPV_SHA384_CTX *, char[SHA384_DIGEST_STRING_LENGTH]);
char *netpgpv_SHA384_FileChunk(const char *, char *, off_t, off_t);
char *netpgpv_SHA384_File(const char *, char *);
char *netpgpv_SHA384_Data(const uint8_t *, size_t, char[SHA384_DIGEST_STRING_LENGTH]);
#endif /* !_KERNEL */

int netpgpv_SHA512_Init(NETPGPV_SHA512_CTX*);
int netpgpv_SHA512_Update(NETPGPV_SHA512_CTX*, const uint8_t*, size_t);
int netpgpv_SHA512_Final(uint8_t[SHA512_DIGEST_LENGTH], NETPGPV_SHA512_CTX*);
#ifndef _KERNEL
char *netpgpv_SHA512_End(NETPGPV_SHA512_CTX *, char[SHA512_DIGEST_STRING_LENGTH]);
char *netpgpv_SHA512_FileChunk(const char *, char *, off_t, off_t);
char *netpgpv_SHA512_File(const char *, char *);
char *netpgpv_SHA512_Data(const uint8_t *, size_t, char[SHA512_DIGEST_STRING_LENGTH]);
#endif /* !_KERNEL */
__END_DECLS

#endif /* __SHA2_H__ */
