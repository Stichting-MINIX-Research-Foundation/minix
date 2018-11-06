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

#ifdef	USE_BN_INTERFACE
#define	BIGNUM		PGPV_BIGNUM
#define	BN_ULONG	PGPV_BN_ULONG
#define	BN_CTX		PGPV_BN_CTX
#define BN_is_negative	PGPV_BN_is_negative
#define BN_is_zero	PGPV_BN_is_zero
#define BN_is_odd	PGPV_BN_is_odd
#define BN_is_even	PGPV_BN_is_even
#define BN_new		PGPV_BN_new
#define BN_dup		PGPV_BN_dup
#define BN_copy		PGPV_BN_copy
#define BN_init		PGPV_BN_init
#define BN_free		PGPV_BN_free
#define BN_clear	PGPV_BN_clear
#define BN_clear_free	PGPV_BN_clear_free
#define BN_cmp		PGPV_BN_cmp
#define BN_bn2bin	PGPV_BN_bn2bin
#define BN_bn2hex	PGPV_BN_bn2hex
#define BN_bn2dec	PGPV_BN_bn2dec
#define BN_bn2radix	PGPV_BN_bn2radix
#define BN_hex2bn	PGPV_BN_hex2bn
#define BN_dec2bn	PGPV_BN_dec2bn
#define BN_radix2bn	PGPV_BN_radix2bn
#ifndef _KERNEL
#define BN_print_fp	PGPV_BN_print_fp
#endif
#define BN_add		PGPV_BN_add
#define BN_sub		PGPV_BN_sub
#define BN_mul		PGPV_BN_mul
#define BN_div		PGPV_BN_div
#define BN_swap		PGPV_BN_swap
#define BN_bitop	PGPV_BN_bitop
#define BN_lshift	PGPV_BN_lshift
#define BN_lshift1	PGPV_BN_lshift1
#define BN_rshift	PGPV_BN_rshift
#define BN_rshift1	PGPV_BN_rshift1
#define BN_set_word	PGPV_BN_set_word
#define BN_set_negative	PGPV_BN_set_negative
#define BN_num_bytes	PGPV_BN_num_bytes
#define BN_num_bits	PGPV_BN_num_bits
#define BN_mod_exp	PGPV_BN_mod_exp
#define BN_mod_inverse	PGPV_BN_mod_inverse
#define BN_mod_mul	PGPV_BN_mod_mul
#define BN_mod_sub	PGPV_BN_mod_sub
#define BN_raise	PGPV_BN_raise
#define BN_factorial	PGPV_BN_factorial
#define BN_CTX_new	PGPV_BN_CTX_new
#define BN_CTX_get	PGPV_BN_CTX_get
#define BN_CTX_start	PGPV_BN_CTX_start
#define BN_CTX_end	PGPV_BN_CTX_end
#define BN_CTX_init	PGPV_BN_CTX_init
#define BN_CTX_free	PGPV_BN_CTX_free
#define BN_rand		PGPV_BN_rand
#define BN_rand_range	PGPV_BN_rand_range
#define BN_is_prime	PGPV_BN_is_prime
#define BN_value_one	PGPV_BN_value_one
#define BN_is_bit_set	PGPV_BN_is_bit_set
#define BN_gcd		PGPV_BN_gcd
#endif /* USE_BN_INTERFACE */

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

#define PGPV_BIGNUM		mp_int
#define PGPV_BN_ULONG	mp_digit

/* a "context" of mp integers - never really used */
typedef struct bn_ctx_t {
	size_t	  count;
	size_t	  arraysize;
	PGPV_BIGNUM	**v;
} PGPV_BN_CTX;

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

#define PGPV_BN_is_negative(x)	((x)->sign == MP_NEG)
#define PGPV_BN_is_zero(a) 		(((a)->used == 0) ? 1 : 0)
#define PGPV_BN_is_odd(a)  		(((a)->used > 0 && (((a)->dp[0] & 1) == 1)) ? 1 : 0)
#define PGPV_BN_is_even(a) 		(((a)->used > 0 && (((a)->dp[0] & 1) == 0)) ? 1 : 0)

PGPV_BIGNUM *PGPV_BN_new(void);
PGPV_BIGNUM *PGPV_BN_dup(const PGPV_BIGNUM */*a*/);
int PGPV_BN_copy(PGPV_BIGNUM */*b*/, const PGPV_BIGNUM */*a*/);

void PGPV_BN_init(PGPV_BIGNUM */*a*/);
void PGPV_BN_free(PGPV_BIGNUM */*a*/);
void PGPV_BN_clear(PGPV_BIGNUM */*a*/);
void PGPV_BN_clear_free(PGPV_BIGNUM */*a*/);

int PGPV_BN_cmp(PGPV_BIGNUM */*a*/, PGPV_BIGNUM */*b*/);

PGPV_BIGNUM *PGPV_BN_bin2bn(const uint8_t */*buf*/, int /*size*/, PGPV_BIGNUM */*bn*/);
int PGPV_BN_bn2bin(const PGPV_BIGNUM */*a*/, unsigned char */*b*/);
char *PGPV_BN_bn2hex(const PGPV_BIGNUM */*a*/);
char *PGPV_BN_bn2dec(const PGPV_BIGNUM */*a*/);
char *PGPV_BN_bn2radix(const PGPV_BIGNUM */*a*/, unsigned /*radix*/);
int PGPV_BN_hex2bn(PGPV_BIGNUM **/*a*/, const char */*str*/);
int PGPV_BN_dec2bn(PGPV_BIGNUM **/*a*/, const char */*str*/);
int PGPV_BN_radix2bn(PGPV_BIGNUM **/*a*/, const char */*str*/, unsigned /*radix*/);
#ifndef _KERNEL
int PGPV_BN_print_fp(FILE */*fp*/, const PGPV_BIGNUM */*a*/);
#endif

int PGPV_BN_add(PGPV_BIGNUM */*r*/, const PGPV_BIGNUM */*a*/, const PGPV_BIGNUM */*b*/);
int PGPV_BN_sub(PGPV_BIGNUM */*r*/, const PGPV_BIGNUM */*a*/, const PGPV_BIGNUM */*b*/);
int PGPV_BN_mul(PGPV_BIGNUM */*r*/, const PGPV_BIGNUM */*a*/, const PGPV_BIGNUM */*b*/, PGPV_BN_CTX */*ctx*/);
int PGPV_BN_div(PGPV_BIGNUM */*q*/, PGPV_BIGNUM */*r*/, const PGPV_BIGNUM */*a*/, const PGPV_BIGNUM */*b*/, PGPV_BN_CTX */*ctx*/);
void PGPV_BN_swap(PGPV_BIGNUM */*a*/, PGPV_BIGNUM */*b*/);
int PGPV_BN_bitop(PGPV_BIGNUM */*r*/, const PGPV_BIGNUM */*a*/, char /*op*/, const PGPV_BIGNUM */*b*/);
int PGPV_BN_lshift(PGPV_BIGNUM */*r*/, const PGPV_BIGNUM */*a*/, int /*n*/);
int PGPV_BN_lshift1(PGPV_BIGNUM */*r*/, PGPV_BIGNUM */*a*/);
int PGPV_BN_rshift(PGPV_BIGNUM */*r*/, const PGPV_BIGNUM */*a*/, int /*n*/);
int PGPV_BN_rshift1(PGPV_BIGNUM */*r*/, PGPV_BIGNUM */*a*/);
int PGPV_BN_set_word(PGPV_BIGNUM */*a*/, PGPV_BN_ULONG /*w*/);
void PGPV_BN_set_negative(PGPV_BIGNUM */*a*/, int /*n*/);

int PGPV_BN_num_bytes(const PGPV_BIGNUM */*a*/);
int PGPV_BN_num_bits(const PGPV_BIGNUM */*a*/);

int PGPV_BN_mod_exp(PGPV_BIGNUM */*r*/, PGPV_BIGNUM */*a*/, PGPV_BIGNUM */*p*/, PGPV_BIGNUM */*m*/, PGPV_BN_CTX */*ctx*/);
PGPV_BIGNUM *PGPV_BN_mod_inverse(PGPV_BIGNUM */*ret*/, PGPV_BIGNUM */*a*/, const PGPV_BIGNUM */*n*/, PGPV_BN_CTX */*ctx*/);
int PGPV_BN_mod_mul(PGPV_BIGNUM */*ret*/, PGPV_BIGNUM */*a*/, PGPV_BIGNUM */*b*/, const PGPV_BIGNUM */*m*/, PGPV_BN_CTX */*ctx*/);
int PGPV_BN_mod_sub(PGPV_BIGNUM */*r*/, PGPV_BIGNUM */*a*/, PGPV_BIGNUM */*b*/, const PGPV_BIGNUM */*m*/, PGPV_BN_CTX */*ctx*/);

int PGPV_BN_raise(PGPV_BIGNUM */*res*/, PGPV_BIGNUM */*a*/, PGPV_BIGNUM */*b*/);
int PGPV_BN_factorial(PGPV_BIGNUM */*fact*/, PGPV_BIGNUM */*f*/);

PGPV_BN_CTX *PGPV_BN_CTX_new(void);
PGPV_BIGNUM *PGPV_BN_CTX_get(PGPV_BN_CTX */*ctx*/);
void PGPV_BN_CTX_start(PGPV_BN_CTX */*ctx*/);
void PGPV_BN_CTX_end(PGPV_BN_CTX */*ctx*/);
void PGPV_BN_CTX_init(PGPV_BN_CTX */*c*/);
void PGPV_BN_CTX_free(PGPV_BN_CTX */*c*/);

int PGPV_BN_rand(PGPV_BIGNUM */*rnd*/, int /*bits*/, int /*top*/, int /*bottom*/);
int PGPV_BN_rand_range(PGPV_BIGNUM */*rnd*/, PGPV_BIGNUM */*range*/);

int PGPV_BN_is_prime(const PGPV_BIGNUM */*a*/, int /*checks*/, void (*callback)(int, int, void *), PGPV_BN_CTX */*ctx*/, void */*cb_arg*/);

const PGPV_BIGNUM *PGPV_BN_value_one(void);
int PGPV_BN_is_bit_set(const PGPV_BIGNUM */*a*/, int /*n*/);

int PGPV_BN_gcd(PGPV_BIGNUM */*r*/, PGPV_BIGNUM */*a*/, PGPV_BIGNUM */*b*/, PGPV_BN_CTX */*ctx*/);

__END_DECLS

#endif
