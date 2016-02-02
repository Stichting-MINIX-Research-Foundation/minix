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
#ifndef RSA_H_
#define RSA_H_	20120325

#include "bn.h"

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

typedef struct rsa_pubkey_t {
	BIGNUM		*n;	/* RSA public modulus n */
	BIGNUM		*e;	/* RSA public encryption exponent e */
} rsa_pubkey_t;

typedef struct mpi_rsa_t {
	int		 f1;	/* openssl pad */
	long		 f2;	/* openssl version */
	const void	*f3;	/* openssl method */
	void		*f4;	/* openssl engine */
	BIGNUM		*n;
	BIGNUM		*e;
	BIGNUM		*d;
	BIGNUM		*p;
	BIGNUM		*q;
	BIGNUM		*dmp1;
	BIGNUM		*dmq1;
	BIGNUM		*iqmp;
} mpi_rsa_t;

#define RSA	mpi_rsa_t

typedef struct dsa_pubkey_t {
	BIGNUM		*p;	/* DSA public modulus n */
	BIGNUM		*q;	/* DSA public encryption exponent e */
	BIGNUM		*g;
	BIGNUM		*y;
} dsa_pubkey_t;

typedef struct mpi_dsa_t {
	BIGNUM		*p;
	BIGNUM		*q;
	BIGNUM		*g;
	BIGNUM		*y;
	BIGNUM		*x;
	BIGNUM		*pub_key;
	BIGNUM		*priv_key;
} mpi_dsa_t;

#define DSA	mpi_dsa_t

typedef struct rsasig_t {
	BIGNUM		*sig;			/* mpi which is actual signature */
} rsasig_t;

typedef struct dsasig_t {
	BIGNUM		*r;			/* mpi which is actual signature */
	BIGNUM		*s;			/* mpi which is actual signature */
} dsasig_t;

#define DSA_SIG		dsasig_t

/* misc defs */
#define RSA_NO_PADDING			3

#define SIGNETBSD_ID_SIZE		8
#define SIGNETBSD_NAME_SIZE		128

#define RSA_PUBKEY_ALG			1
#define DSA_PUBKEY_ALG			17

/* the public part of the key */
typedef struct pubkey_t {
	uint32_t	version;		/* key version - usually 4 */
	uint8_t		id[SIGNETBSD_ID_SIZE];		/* binary id */
	char		name[SIGNETBSD_NAME_SIZE];	/* name of identity - not necessary, but looks better */
	int64_t		birthtime;		/* time of creation of key */
	int64_t		expiry;			/* expiration time of the key */
	uint32_t	validity;		/* validity in days */
	uint32_t	alg;			/* pubkey algorithm - rsa/dss etc */
	rsa_pubkey_t	rsa;			/* specific RSA keys */
	dsa_pubkey_t	dsa;			/* specific DSA keys */
} pubkey_t;

/* signature details (for a specific file) */
typedef struct signature_t {
	uint32_t	 version;		/* signature version number */
	uint32_t	 type;			/* signature type value */
	int64_t		 birthtime;		/* creation time of the signature */
	int64_t		 expiry;		/* expiration time of the signature */
	uint8_t		 id[SIGNETBSD_ID_SIZE];	/* binary id */
	uint32_t	 key_alg;		/* public key algorithm number */
	uint32_t	 hash_alg;		/* hashing algorithm number */
	rsasig_t	 rsa;			/* RSA signature */
	dsasig_t	 dsa;			/* DSA signature */
	size_t           v4_hashlen;		/* length of hashed info */
	uint8_t		*v4_hashed;		/* hashed info */
	uint8_t		 hash2[2];		/* high 2 bytes of hashed value - for quick test */
	pubkey_t	*signer;		/* pubkey of signer */
} signature_t;

unsigned dsa_verify(const signature_t */*sig*/, const dsa_pubkey_t */*pubdsa*/, const uint8_t */*calc*/, size_t /*hashlen*/);

RSA *RSA_new(void);
int RSA_size(const RSA */*rsa*/);
void RSA_free(RSA */*rsa*/);
int RSA_check_key(RSA */*rsa*/);
RSA *RSA_generate_key(int /*num*/, unsigned long /*e*/, void (*callback)(int,int,void *), void */*cb_arg*/);
int RSA_public_encrypt(int flen, const unsigned char *from, unsigned char *to, RSA *rsa, int padding);
int RSA_private_decrypt(int flen, const unsigned char *from, unsigned char *to, RSA *rsa, int padding);
int RSA_private_encrypt(int flen, const unsigned char *from, unsigned char *to, RSA *rsa, int padding);
int RSA_public_decrypt(int flen, const uint8_t *from, uint8_t *to, RSA *rsa, int padding);

DSA *DSA_new(void);
int DSA_size(const DSA */*rsa*/);
void DSA_free(DSA */*dsa*/);
DSA_SIG *DSA_SIG_new(void);
void DSA_SIG_free(DSA_SIG */*sig*/);
int DSA_do_verify(const unsigned char *dgst, int dgst_len, DSA_SIG *sig, DSA *dsa);
DSA_SIG *DSA_do_sign(const unsigned char *dgst, int dlen, DSA *dsa);

__END_DECLS

#endif
