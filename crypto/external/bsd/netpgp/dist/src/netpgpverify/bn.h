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
#ifndef FAUXBN_H_
#define FAUXBN_H_	20100108

#include <sys/types.h>

#ifndef _KERNEL
# include <inttypes.h>
# include <stdio.h>
#endif

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

/* should be 32bit on ILP32, 64bit on LP64 */
typedef unsigned long	mp_digit;
typedef uint64_t	mp_word;

/* multi-precision integer */
typedef struct mp_int {
	mp_digit	*dp;	/* array of digits */
	int		 used;	/* # of digits used */
	int		 alloc;	/* # of digits allocated */
	int		 sign;	/* non-zero if negative */
} mp_int;

#define BIGNUM		mp_int
#define BN_ULONG	mp_digit

/* a "context" of mp integers - never really used */
typedef struct bn_ctx_t {
	size_t	  count;
	size_t	  arraysize;
	BIGNUM	**v;
} BN_CTX;

#define MP_LT		-1
#define MP_EQ		0
#define MP_GT		1

#define MP_ZPOS		0
#define MP_NEG		1

#define MP_OKAY		0
#define MP_MEM		-2
#define MP_VAL		-3
#define MP_RANGE	MP_VAL

/*********************************/

#define BN_is_negative(x)	((x)->sign == MP_NEG)
#define BN_is_zero(a) 		(((a)->used == 0) ? 1 : 0)
#define BN_is_odd(a)  		(((a)->used > 0 && (((a)->dp[0] & 1) == 1)) ? 1 : 0)
#define BN_is_even(a) 		(((a)->used > 0 && (((a)->dp[0] & 1) == 0)) ? 1 : 0)

BIGNUM *BN_new(void);
BIGNUM *BN_dup(const BIGNUM */*a*/);
int BN_copy(BIGNUM */*b*/, const BIGNUM */*a*/);

void BN_init(BIGNUM */*a*/);
void BN_free(BIGNUM */*a*/);
void BN_clear(BIGNUM */*a*/);
void BN_clear_free(BIGNUM */*a*/);

int BN_cmp(BIGNUM */*a*/, BIGNUM */*b*/);

BIGNUM *BN_bin2bn(const uint8_t */*buf*/, int /*size*/, BIGNUM */*bn*/);
int BN_bn2bin(const BIGNUM */*a*/, unsigned char */*b*/);
char *BN_bn2hex(const BIGNUM */*a*/);
char *BN_bn2dec(const BIGNUM */*a*/);
char *BN_bn2radix(const BIGNUM */*a*/, unsigned /*radix*/);
int BN_hex2bn(BIGNUM **/*a*/, const char */*str*/);
int BN_dec2bn(BIGNUM **/*a*/, const char */*str*/);
int BN_radix2bn(BIGNUM **/*a*/, const char */*str*/, unsigned /*radix*/);
#ifndef _KERNEL
int BN_print_fp(FILE */*fp*/, const BIGNUM */*a*/);
#endif

int BN_add(BIGNUM */*r*/, const BIGNUM */*a*/, const BIGNUM */*b*/);
int BN_sub(BIGNUM */*r*/, const BIGNUM */*a*/, const BIGNUM */*b*/);
int BN_mul(BIGNUM */*r*/, const BIGNUM */*a*/, const BIGNUM */*b*/, BN_CTX */*ctx*/);
int BN_div(BIGNUM */*q*/, BIGNUM */*r*/, const BIGNUM */*a*/, const BIGNUM */*b*/, BN_CTX */*ctx*/);
void BN_swap(BIGNUM */*a*/, BIGNUM */*b*/);
int BN_bitop(BIGNUM */*r*/, const BIGNUM */*a*/, char /*op*/, const BIGNUM */*b*/);
int BN_lshift(BIGNUM */*r*/, const BIGNUM */*a*/, int /*n*/);
int BN_lshift1(BIGNUM */*r*/, BIGNUM */*a*/);
int BN_rshift(BIGNUM */*r*/, const BIGNUM */*a*/, int /*n*/);
int BN_rshift1(BIGNUM */*r*/, BIGNUM */*a*/);
int BN_set_word(BIGNUM */*a*/, BN_ULONG /*w*/);
void BN_set_negative(BIGNUM */*a*/, int /*n*/);

int BN_num_bytes(const BIGNUM */*a*/);
int BN_num_bits(const BIGNUM */*a*/);

int BN_mod_exp(BIGNUM */*r*/, BIGNUM */*a*/, BIGNUM */*p*/, BIGNUM */*m*/, BN_CTX */*ctx*/);
BIGNUM *BN_mod_inverse(BIGNUM */*ret*/, BIGNUM */*a*/, const BIGNUM */*n*/, BN_CTX */*ctx*/);
int BN_mod_mul(BIGNUM */*ret*/, BIGNUM */*a*/, BIGNUM */*b*/, const BIGNUM */*m*/, BN_CTX */*ctx*/);
int BN_mod_sub(BIGNUM */*r*/, BIGNUM */*a*/, BIGNUM */*b*/, const BIGNUM */*m*/, BN_CTX */*ctx*/);

int BN_raise(BIGNUM */*res*/, BIGNUM */*a*/, BIGNUM */*b*/);
int BN_factorial(BIGNUM */*fact*/, BIGNUM */*f*/);

BN_CTX *BN_CTX_new(void);
BIGNUM *BN_CTX_get(BN_CTX */*ctx*/);
void BN_CTX_start(BN_CTX */*ctx*/);
void BN_CTX_end(BN_CTX */*ctx*/);
void BN_CTX_init(BN_CTX */*c*/);
void BN_CTX_free(BN_CTX */*c*/);

int BN_rand(BIGNUM */*rnd*/, int /*bits*/, int /*top*/, int /*bottom*/);
int BN_rand_range(BIGNUM */*rnd*/, BIGNUM */*range*/);

int BN_is_prime(const BIGNUM */*a*/, int /*checks*/, void (*callback)(int, int, void *), BN_CTX */*ctx*/, void */*cb_arg*/);

const BIGNUM *BN_value_one(void);
int BN_is_bit_set(const BIGNUM */*a*/, int /*n*/);

__END_DECLS

#endif
