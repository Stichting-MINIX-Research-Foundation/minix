/*-
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Alistair Crooks (agc@netbsd.org)
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef NETPGPDIGEST_H_
#define NETPGPDIGEST_H_

/* header file to define the sizes for various digest arrays */

#ifdef HAVE_OPENSSL_MD5_H
#include <openssl/md5.h>
#endif

#ifdef HAVE_OPENSSL_SHA_H
#include <openssl/sha.h>
#endif

/* Apple */
#ifdef HAVE_COMMONCRYPTO_COMMONDIGEST_H
#undef MD5_DIGEST_LENGTH
#undef SHA_DIGEST_LENGTH
#define COMMON_DIGEST_FOR_OPENSSL	1
#include <CommonCrypto/CommonDigest.h>
#endif

/* SHA1 Hash Size */
#define PGP_SHA1_HASH_SIZE 	SHA_DIGEST_LENGTH
#define PGP_SHA256_HASH_SIZE	SHA256_DIGEST_LENGTH
#define PGP_CHECKHASH_SIZE	PGP_SHA1_HASH_SIZE

#endif /* NETPGPDIGEST_H_ */
