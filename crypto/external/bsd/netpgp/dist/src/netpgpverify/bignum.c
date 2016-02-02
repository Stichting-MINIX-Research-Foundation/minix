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
/* LibTomMath, multiple-precision integer library -- Tom St Denis
 *
 * LibTomMath is a library that provides multiple-precision
 * integer arithmetic as well as number theoretic functionality.
 *
 * The library was designed directly after the MPI library by
 * Michael Fromberger but has been written from scratch with
 * additional optimizations in place.
 *
 * The library is free for all purposes without any express
 * guarantee it works.
 *
 * Tom St Denis, tomstdenis@gmail.com, http://libtom.org
 */
#include "config.h"

#include <sys/types.h>
#include <sys/param.h>

#ifdef _KERNEL
# include <sys/kmem.h>
#else
# include <arpa/inet.h>
# include <stdarg.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
#endif

#include "bn.h"

/**************************************************************************/

/* LibTomMath, multiple-precision integer library -- Tom St Denis
 *
 * LibTomMath is a library that provides multiple-precision
 * integer arithmetic as well as number theoretic functionality.
 *
 * The library was designed directly after the MPI library by
 * Michael Fromberger but has been written from scratch with
 * additional optimizations in place.
 *
 * The library is free for all purposes without any express
 * guarantee it works.
 *
 * Tom St Denis, tomstdenis@gmail.com, http://libtom.org
 */

#define MP_PREC		32
#define DIGIT_BIT	28
#define MP_MASK          ((((mp_digit)1)<<((mp_digit)DIGIT_BIT))-((mp_digit)1))

#define MP_WARRAY	/*LINTED*/(1U << (((sizeof(mp_word) * CHAR_BIT) - (2 * DIGIT_BIT) + 1)))

#define MP_NO		0
#define MP_YES		1

#ifndef USE_ARG
#define USE_ARG(x)	/*LINTED*/(void)&(x)
#endif

#ifndef __arraycount
#define	__arraycount(__x)	(sizeof(__x) / sizeof(__x[0]))
#endif

#define MP_ISZERO(a) (((a)->used == 0) ? MP_YES : MP_NO)

typedef int           mp_err;

static int signed_multiply(mp_int * a, mp_int * b, mp_int * c);
static int square(mp_int * a, mp_int * b);

static int signed_subtract_word(mp_int *a, mp_digit b, mp_int *c);

static inline void *
allocate(size_t n, size_t m)
{
	return calloc(n, m);
}

static inline void
deallocate(void *v, size_t sz)
{
	USE_ARG(sz);
	free(v);
}

/* set to zero */
static inline void
mp_zero(mp_int *a)
{
	a->sign = MP_ZPOS;
	a->used = 0;
	memset(a->dp, 0x0, a->alloc * sizeof(*a->dp));
}

/* grow as required */
static int
mp_grow(mp_int *a, int size)
{
	mp_digit *tmp;

	/* if the alloc size is smaller alloc more ram */
	if (a->alloc < size) {
		/* ensure there are always at least MP_PREC digits extra on top */
		size += (MP_PREC * 2) - (size % MP_PREC);

		/* reallocate the array a->dp
		*
		* We store the return in a temporary variable
		* in case the operation failed we don't want
		* to overwrite the dp member of a.
		*/
		tmp = realloc(a->dp, sizeof(*tmp) * size);
		if (tmp == NULL) {
			/* reallocation failed but "a" is still valid [can be freed] */
			return MP_MEM;
		}

		/* reallocation succeeded so set a->dp */
		a->dp = tmp;
		/* zero excess digits */
		memset(&a->dp[a->alloc], 0x0, (size - a->alloc) * sizeof(*a->dp));
		a->alloc = size;
	}
	return MP_OKAY;
}

/* shift left a certain amount of digits */
static int
lshift_digits(mp_int * a, int b)
{
	mp_digit *top, *bottom;
	int     x, res;

	/* if its less than zero return */
	if (b <= 0) {
		return MP_OKAY;
	}

	/* grow to fit the new digits */
	if (a->alloc < a->used + b) {
		if ((res = mp_grow(a, a->used + b)) != MP_OKAY) {
			return res;
		}
	}

	/* increment the used by the shift amount then copy upwards */
	a->used += b;

	/* top */
	top = a->dp + a->used - 1;

	/* base */
	bottom = a->dp + a->used - 1 - b;

	/* much like rshift_digits this is implemented using a sliding window
	* except the window goes the otherway around.  Copying from
	* the bottom to the top.
	*/
	for (x = a->used - 1; x >= b; x--) {
		*top-- = *bottom--;
	}

	/* zero the lower digits */
	memset(a->dp, 0x0, b * sizeof(*a->dp));
	return MP_OKAY;
}

/* trim unused digits 
 *
 * This is used to ensure that leading zero digits are
 * trimed and the leading "used" digit will be non-zero
 * Typically very fast.  Also fixes the sign if there
 * are no more leading digits
 */
static void
trim_unused_digits(mp_int * a)
{
	/* decrease used while the most significant digit is
	* zero.
	*/
	while (a->used > 0 && a->dp[a->used - 1] == 0) {
		a->used -= 1;
	}
	/* reset the sign flag if used == 0 */
	if (a->used == 0) {
		a->sign = MP_ZPOS;
	}
}

/* copy, b = a */
static int
mp_copy(BIGNUM *a, BIGNUM *b)
{
	int	res;

	/* if dst == src do nothing */
	if (a == b) {
		return MP_OKAY;
	}
	if (a == NULL || b == NULL) {
		return MP_VAL;
	}

	/* grow dest */
	if (b->alloc < a->used) {
		if ((res = mp_grow(b, a->used)) != MP_OKAY) {
			return res;
		}
	}

	memcpy(b->dp, a->dp, a->used * sizeof(*b->dp));
	if (b->used > a->used) {
		memset(&b->dp[a->used], 0x0, (b->used - a->used) * sizeof(*b->dp));
	}

	/* copy used count and sign */
	b->used = a->used;
	b->sign = a->sign;
	return MP_OKAY;
}

/* shift left by a certain bit count */
static int
lshift_bits(mp_int *a, int b, mp_int *c)
{
	mp_digit d;
	int      res;

	/* copy */
	if (a != c) {
		if ((res = mp_copy(a, c)) != MP_OKAY) {
			return res;
		}
	}

	if (c->alloc < (int)(c->used + b/DIGIT_BIT + 1)) {
		if ((res = mp_grow(c, c->used + b / DIGIT_BIT + 1)) != MP_OKAY) {
			return res;
		}
	}

	/* shift by as many digits in the bit count */
	if (b >= (int)DIGIT_BIT) {
		if ((res = lshift_digits(c, b / DIGIT_BIT)) != MP_OKAY) {
			return res;
		}
	}

	/* shift any bit count < DIGIT_BIT */
	d = (mp_digit) (b % DIGIT_BIT);
	if (d != 0) {
		mp_digit *tmpc, shift, mask, carry, rr;
		int x;

		/* bitmask for carries */
		mask = (((mp_digit)1) << d) - 1;

		/* shift for msbs */
		shift = DIGIT_BIT - d;

		/* alias */
		tmpc = c->dp;

		/* carry */
		carry = 0;
		for (x = 0; x < c->used; x++) {
			/* get the higher bits of the current word */
			rr = (*tmpc >> shift) & mask;

			/* shift the current word and OR in the carry */
			*tmpc = ((*tmpc << d) | carry) & MP_MASK;
			++tmpc;

			/* set the carry to the carry bits of the current word */
			carry = rr;
		}

		/* set final carry */
		if (carry != 0) {
			c->dp[c->used++] = carry;
		}
	}
	trim_unused_digits(c);
	return MP_OKAY;
}

/* reads a unsigned char array, assumes the msb is stored first [big endian] */
static int
mp_read_unsigned_bin(mp_int *a, const uint8_t *b, int c)
{
	int     res;

	/* make sure there are at least two digits */
	if (a->alloc < 2) {
		if ((res = mp_grow(a, 2)) != MP_OKAY) {
			return res;
		}
	}

	/* zero the int */
	mp_zero(a);

	/* read the bytes in */
	while (c-- > 0) {
		if ((res = lshift_bits(a, 8, a)) != MP_OKAY) {
			return res;
		}

		a->dp[0] |= *b++;
		a->used += 1;
	}
	trim_unused_digits(a);
	return MP_OKAY;
}

/* returns the number of bits in an mpi */
static int
mp_count_bits(const mp_int *a)
{
	int     r;
	mp_digit q;

	/* shortcut */
	if (a->used == 0) {
		return 0;
	}

	/* get number of digits and add that */
	r = (a->used - 1) * DIGIT_BIT;

	/* take the last digit and count the bits in it */
	for (q = a->dp[a->used - 1]; q > ((mp_digit) 0) ; r++) {
		q >>= ((mp_digit) 1);
	}
	return r;
}

/* compare maginitude of two ints (unsigned) */
static int
compare_magnitude(mp_int * a, mp_int * b)
{
	int     n;
	mp_digit *tmpa, *tmpb;

	/* compare based on # of non-zero digits */
	if (a->used > b->used) {
		return MP_GT;
	}

	if (a->used < b->used) {
		return MP_LT;
	}

	/* alias for a */
	tmpa = a->dp + (a->used - 1);

	/* alias for b */
	tmpb = b->dp + (a->used - 1);

	/* compare based on digits  */
	for (n = 0; n < a->used; ++n, --tmpa, --tmpb) {
		if (*tmpa > *tmpb) {
			return MP_GT;
		}

		if (*tmpa < *tmpb) {
			return MP_LT;
		}
	}
	return MP_EQ;
}

/* compare two ints (signed)*/
static int
signed_compare(mp_int * a, mp_int * b)
{
	/* compare based on sign */
	if (a->sign != b->sign) {
		return (a->sign == MP_NEG) ? MP_LT : MP_GT;
	}
	return (a->sign == MP_NEG) ? compare_magnitude(b, a) : compare_magnitude(a, b);
}

/* get the size for an unsigned equivalent */
static int
mp_unsigned_bin_size(mp_int * a)
{
	int     size = mp_count_bits(a);

	return (size / 8 + ((size & 7) != 0 ? 1 : 0));
}

/* init a new mp_int */
static int
mp_init(mp_int * a)
{
	/* allocate memory required and clear it */
	a->dp = allocate(1, sizeof(*a->dp) * MP_PREC);
	if (a->dp == NULL) {
		return MP_MEM;
	}

	/* set the digits to zero */
	memset(a->dp, 0x0, MP_PREC * sizeof(*a->dp));

	/* set the used to zero, allocated digits to the default precision
	* and sign to positive */
	a->used  = 0;
	a->alloc = MP_PREC;
	a->sign  = MP_ZPOS;

	return MP_OKAY;
}

/* clear one (frees)  */
static void
mp_clear(mp_int * a)
{
	/* only do anything if a hasn't been freed previously */
	if (a->dp != NULL) {
		memset(a->dp, 0x0, a->used * sizeof(*a->dp));

		/* free ram */
		deallocate(a->dp, (size_t)a->alloc);

		/* reset members to make debugging easier */
		a->dp = NULL;
		a->alloc = a->used = 0;
		a->sign  = MP_ZPOS;
	}
}

static int
mp_init_multi(mp_int *mp, ...) 
{
	mp_err res = MP_OKAY;      /* Assume ok until proven otherwise */
	int n = 0;                 /* Number of ok inits */
	mp_int* cur_arg = mp;
	va_list args;

	va_start(args, mp);        /* init args to next argument from caller */
	while (cur_arg != NULL) {
		if (mp_init(cur_arg) != MP_OKAY) {
			/* Oops - error! Back-track and mp_clear what we already
			succeeded in init-ing, then return error.
			*/
			va_list clean_args;

			/* end the current list */
			va_end(args);

			/* now start cleaning up */            
			cur_arg = mp;
			va_start(clean_args, mp);
			while (n--) {
				mp_clear(cur_arg);
				cur_arg = va_arg(clean_args, mp_int*);
			}
			va_end(clean_args);
			res = MP_MEM;
			break;
		}
		n++;
		cur_arg = va_arg(args, mp_int*);
	}
	va_end(args);
	return res;                /* Assumed ok, if error flagged above. */
}

/* init an mp_init for a given size */
static int
mp_init_size(mp_int * a, int size)
{
	/* pad size so there are always extra digits */
	size += (MP_PREC * 2) - (size % MP_PREC);	

	/* alloc mem */
	a->dp = allocate(1, sizeof(*a->dp) * size);
	if (a->dp == NULL) {
		return MP_MEM;
	}

	/* set the members */
	a->used  = 0;
	a->alloc = size;
	a->sign  = MP_ZPOS;

	/* zero the digits */
	memset(a->dp, 0x0, size * sizeof(*a->dp));
	return MP_OKAY;
}

/* creates "a" then copies b into it */
static int
mp_init_copy(mp_int * a, mp_int * b)
{
	int     res;

	if ((res = mp_init(a)) != MP_OKAY) {
		return res;
	}
	return mp_copy(b, a);
}

/* low level addition, based on HAC pp.594, Algorithm 14.7 */
static int
basic_add(mp_int * a, mp_int * b, mp_int * c)
{
	mp_int *x;
	int     olduse, res, min, max;

	/* find sizes, we let |a| <= |b| which means we have to sort
	* them.  "x" will point to the input with the most digits
	*/
	if (a->used > b->used) {
		min = b->used;
		max = a->used;
		x = a;
	} else {
		min = a->used;
		max = b->used;
		x = b;
	}

	/* init result */
	if (c->alloc < max + 1) {
		if ((res = mp_grow(c, max + 1)) != MP_OKAY) {
			return res;
		}
	}

	/* get old used digit count and set new one */
	olduse = c->used;
	c->used = max + 1;

	{
		mp_digit carry, *tmpa, *tmpb, *tmpc;
		int i;

		/* alias for digit pointers */

		/* first input */
		tmpa = a->dp;

		/* second input */
		tmpb = b->dp;

		/* destination */
		tmpc = c->dp;

		/* zero the carry */
		carry = 0;
		for (i = 0; i < min; i++) {
			/* Compute the sum at one digit, T[i] = A[i] + B[i] + U */
			*tmpc = *tmpa++ + *tmpb++ + carry;

			/* U = carry bit of T[i] */
			carry = *tmpc >> ((mp_digit)DIGIT_BIT);

			/* take away carry bit from T[i] */
			*tmpc++ &= MP_MASK;
		}

		/* now copy higher words if any, that is in A+B 
		* if A or B has more digits add those in 
		*/
		if (min != max) {
			for (; i < max; i++) {
				/* T[i] = X[i] + U */
				*tmpc = x->dp[i] + carry;

				/* U = carry bit of T[i] */
				carry = *tmpc >> ((mp_digit)DIGIT_BIT);

				/* take away carry bit from T[i] */
				*tmpc++ &= MP_MASK;
			}
		}

		/* add carry */
		*tmpc++ = carry;

		/* clear digits above oldused */
		if (olduse > c->used) {
			memset(tmpc, 0x0, (olduse - c->used) * sizeof(*c->dp));
		}
	}

	trim_unused_digits(c);
	return MP_OKAY;
}

/* low level subtraction (assumes |a| > |b|), HAC pp.595 Algorithm 14.9 */
static int
basic_subtract(mp_int * a, mp_int * b, mp_int * c)
{
	int     olduse, res, min, max;

	/* find sizes */
	min = b->used;
	max = a->used;

	/* init result */
	if (c->alloc < max) {
		if ((res = mp_grow(c, max)) != MP_OKAY) {
			return res;
		}
	}
	olduse = c->used;
	c->used = max;

	{
		mp_digit carry, *tmpa, *tmpb, *tmpc;
		int i;

		/* alias for digit pointers */
		tmpa = a->dp;
		tmpb = b->dp;
		tmpc = c->dp;

		/* set carry to zero */
		carry = 0;
		for (i = 0; i < min; i++) {
			/* T[i] = A[i] - B[i] - U */
			*tmpc = *tmpa++ - *tmpb++ - carry;

			/* U = carry bit of T[i]
			* Note this saves performing an AND operation since
			* if a carry does occur it will propagate all the way to the
			* MSB.  As a result a single shift is enough to get the carry
			*/
			carry = *tmpc >> ((mp_digit)(CHAR_BIT * sizeof(mp_digit) - 1));

			/* Clear carry from T[i] */
			*tmpc++ &= MP_MASK;
		}

		/* now copy higher words if any, e.g. if A has more digits than B  */
		for (; i < max; i++) {
			/* T[i] = A[i] - U */
			*tmpc = *tmpa++ - carry;

			/* U = carry bit of T[i] */
			carry = *tmpc >> ((mp_digit)(CHAR_BIT * sizeof(mp_digit) - 1));

			/* Clear carry from T[i] */
			*tmpc++ &= MP_MASK;
		}

		/* clear digits above used (since we may not have grown result above) */
		if (olduse > c->used) {
			memset(tmpc, 0x0, (olduse - c->used) * sizeof(*a->dp));
		}
	}

	trim_unused_digits(c);
	return MP_OKAY;
}

/* high level subtraction (handles signs) */
static int
signed_subtract(mp_int * a, mp_int * b, mp_int * c)
{
	int     sa, sb, res;

	sa = a->sign;
	sb = b->sign;

	if (sa != sb) {
		/* subtract a negative from a positive, OR */
		/* subtract a positive from a negative. */
		/* In either case, ADD their magnitudes, */
		/* and use the sign of the first number. */
		c->sign = sa;
		res = basic_add(a, b, c);
	} else {
		/* subtract a positive from a positive, OR */
		/* subtract a negative from a negative. */
		/* First, take the difference between their */
		/* magnitudes, then... */
		if (compare_magnitude(a, b) != MP_LT) {
			/* Copy the sign from the first */
			c->sign = sa;
			/* The first has a larger or equal magnitude */
			res = basic_subtract(a, b, c);
		} else {
			/* The result has the *opposite* sign from */
			/* the first number. */
			c->sign = (sa == MP_ZPOS) ? MP_NEG : MP_ZPOS;
			/* The second has a larger magnitude */
			res = basic_subtract(b, a, c);
		}
	}
	return res;
}

/* shift right a certain amount of digits */
static int
rshift_digits(mp_int * a, int b)
{
	/* if b <= 0 then ignore it */
	if (b <= 0) {
		return 0;
	}

	/* if b > used then simply zero it and return */
	if (a->used <= b) {
		mp_zero(a);
		return 0;
	}

	/* this is implemented as a sliding window where 
	* the window is b-digits long and digits from 
	* the top of the window are copied to the bottom
	*
	* e.g.

	b-2 | b-1 | b0 | b1 | b2 | ... | bb |   ---->
		 /\                   |      ---->
		  \-------------------/      ---->
	*/
	memmove(a->dp, &a->dp[b], (a->used - b) * sizeof(*a->dp));
	memset(&a->dp[a->used - b], 0x0, b * sizeof(*a->dp));

	/* remove excess digits */
	a->used -= b;
	return 1;
}

/* multiply by a digit */
static int
multiply_digit(mp_int * a, mp_digit b, mp_int * c)
{
	mp_digit carry, *tmpa, *tmpc;
	mp_word  r;
	int      ix, res, olduse;

	/* make sure c is big enough to hold a*b */
	if (c->alloc < a->used + 1) {
		if ((res = mp_grow(c, a->used + 1)) != MP_OKAY) {
			return res;
		}
	}

	/* get the original destinations used count */
	olduse = c->used;

	/* set the sign */
	c->sign = a->sign;

	/* alias for a->dp [source] */
	tmpa = a->dp;

	/* alias for c->dp [dest] */
	tmpc = c->dp;

	/* zero carry */
	carry = 0;

	/* compute columns */
	for (ix = 0; ix < a->used; ix++) {
		/* compute product and carry sum for this term */
		r = ((mp_word) carry) + ((mp_word)*tmpa++) * ((mp_word)b);

		/* mask off higher bits to get a single digit */
		*tmpc++ = (mp_digit) (r & ((mp_word) MP_MASK));

		/* send carry into next iteration */
		carry = (mp_digit) (r >> ((mp_word) DIGIT_BIT));
	}

	/* store final carry [if any] and increment ix offset  */
	*tmpc++ = carry;
	++ix;
	if (olduse > ix) {
		memset(tmpc, 0x0, (olduse - ix) * sizeof(*tmpc));
	}

	/* set used count */
	c->used = a->used + 1;
	trim_unused_digits(c);

	return MP_OKAY;
}

/* high level addition (handles signs) */
static int
signed_add(mp_int * a, mp_int * b, mp_int * c)
{
	int     asign, bsign, res;

	/* get sign of both inputs */
	asign = a->sign;
	bsign = b->sign;

	/* handle two cases, not four */
	if (asign == bsign) {
		/* both positive or both negative */
		/* add their magnitudes, copy the sign */
		c->sign = asign;
		res = basic_add(a, b, c);
	} else {
		/* one positive, the other negative */
		/* subtract the one with the greater magnitude from */
		/* the one of the lesser magnitude.  The result gets */
		/* the sign of the one with the greater magnitude. */
		if (compare_magnitude(a, b) == MP_LT) {
			c->sign = bsign;
			res = basic_subtract(b, a, c);
		} else {
			c->sign = asign;
			res = basic_subtract(a, b, c);
		}
	}
	return res;
}

/* swap the elements of two integers, for cases where you can't simply swap the 
 * mp_int pointers around
 */
static void
mp_exch(mp_int *a, mp_int *b)
{
	mp_int  t;

	t  = *a;
	*a = *b;
	*b = t;
}

/* calc a value mod 2**b */
static int
modulo_2_to_power(mp_int * a, int b, mp_int * c)
{
	int     x, res;

	/* if b is <= 0 then zero the int */
	if (b <= 0) {
		mp_zero(c);
		return MP_OKAY;
	}

	/* if the modulus is larger than the value than return */
	if (b >= (int) (a->used * DIGIT_BIT)) {
		res = mp_copy(a, c);
		return res;
	}

	/* copy */
	if ((res = mp_copy(a, c)) != MP_OKAY) {
		return res;
	}

	/* zero digits above the last digit of the modulus */
	for (x = (b / DIGIT_BIT) + ((b % DIGIT_BIT) == 0 ? 0 : 1); x < c->used; x++) {
		c->dp[x] = 0;
	}
	/* clear the digit that is not completely outside/inside the modulus */
	c->dp[b / DIGIT_BIT] &=
		(mp_digit) ((((mp_digit) 1) << (((mp_digit) b) % DIGIT_BIT)) - ((mp_digit) 1));
	trim_unused_digits(c);
	return MP_OKAY;
}

/* shift right by a certain bit count (store quotient in c, optional remainder in d) */
static int
rshift_bits(mp_int * a, int b, mp_int * c, mp_int * d)
{
	mp_digit D, r, rr;
	int     x, res;
	mp_int  t;


	/* if the shift count is <= 0 then we do no work */
	if (b <= 0) {
		res = mp_copy(a, c);
		if (d != NULL) {
			mp_zero(d);
		}
		return res;
	}

	if ((res = mp_init(&t)) != MP_OKAY) {
		return res;
	}

	/* get the remainder */
	if (d != NULL) {
		if ((res = modulo_2_to_power(a, b, &t)) != MP_OKAY) {
			mp_clear(&t);
			return res;
		}
	}

	/* copy */
	if ((res = mp_copy(a, c)) != MP_OKAY) {
		mp_clear(&t);
		return res;
	}

	/* shift by as many digits in the bit count */
	if (b >= (int)DIGIT_BIT) {
		rshift_digits(c, b / DIGIT_BIT);
	}

	/* shift any bit count < DIGIT_BIT */
	D = (mp_digit) (b % DIGIT_BIT);
	if (D != 0) {
		mp_digit *tmpc, mask, shift;

		/* mask */
		mask = (((mp_digit)1) << D) - 1;

		/* shift for lsb */
		shift = DIGIT_BIT - D;

		/* alias */
		tmpc = c->dp + (c->used - 1);

		/* carry */
		r = 0;
		for (x = c->used - 1; x >= 0; x--) {
			/* get the lower  bits of this word in a temp */
			rr = *tmpc & mask;

			/* shift the current word and mix in the carry bits from the previous word */
			*tmpc = (*tmpc >> D) | (r << shift);
			--tmpc;

			/* set the carry to the carry bits of the current word found above */
			r = rr;
		}
	}
	trim_unused_digits(c);
	if (d != NULL) {
		mp_exch(&t, d);
	}
	mp_clear(&t);
	return MP_OKAY;
}

/* integer signed division. 
 * c*b + d == a [e.g. a/b, c=quotient, d=remainder]
 * HAC pp.598 Algorithm 14.20
 *
 * Note that the description in HAC is horribly 
 * incomplete.  For example, it doesn't consider 
 * the case where digits are removed from 'x' in 
 * the inner loop.  It also doesn't consider the 
 * case that y has fewer than three digits, etc..
 *
 * The overall algorithm is as described as 
 * 14.20 from HAC but fixed to treat these cases.
*/
static int
signed_divide(mp_int *c, mp_int *d, mp_int *a, mp_int *b)
{
	mp_int  q, x, y, t1, t2;
	int     res, n, t, i, norm, neg;

	/* is divisor zero ? */
	if (MP_ISZERO(b) == MP_YES) {
		return MP_VAL;
	}

	/* if a < b then q=0, r = a */
	if (compare_magnitude(a, b) == MP_LT) {
		if (d != NULL) {
			res = mp_copy(a, d);
		} else {
			res = MP_OKAY;
		}
		if (c != NULL) {
			mp_zero(c);
		}
		return res;
	}

	if ((res = mp_init_size(&q, a->used + 2)) != MP_OKAY) {
		return res;
	}
	q.used = a->used + 2;

	if ((res = mp_init(&t1)) != MP_OKAY) {
		goto LBL_Q;
	}

	if ((res = mp_init(&t2)) != MP_OKAY) {
		goto LBL_T1;
	}

	if ((res = mp_init_copy(&x, a)) != MP_OKAY) {
		goto LBL_T2;
	}

	if ((res = mp_init_copy(&y, b)) != MP_OKAY) {
		goto LBL_X;
	}

	/* fix the sign */
	neg = (a->sign == b->sign) ? MP_ZPOS : MP_NEG;
	x.sign = y.sign = MP_ZPOS;

	/* normalize both x and y, ensure that y >= b/2, [b == 2**DIGIT_BIT] */
	norm = mp_count_bits(&y) % DIGIT_BIT;
	if (norm < (int)(DIGIT_BIT-1)) {
		norm = (DIGIT_BIT-1) - norm;
		if ((res = lshift_bits(&x, norm, &x)) != MP_OKAY) {
			goto LBL_Y;
		}
		if ((res = lshift_bits(&y, norm, &y)) != MP_OKAY) {
			goto LBL_Y;
		}
	} else {
		norm = 0;
	}

	/* note hac does 0 based, so if used==5 then its 0,1,2,3,4, e.g. use 4 */
	n = x.used - 1;
	t = y.used - 1;

	/* while (x >= y*b**n-t) do { q[n-t] += 1; x -= y*b**{n-t} } */
	if ((res = lshift_digits(&y, n - t)) != MP_OKAY) { /* y = y*b**{n-t} */
		goto LBL_Y;
	}

	while (signed_compare(&x, &y) != MP_LT) {
		++(q.dp[n - t]);
		if ((res = signed_subtract(&x, &y, &x)) != MP_OKAY) {
			goto LBL_Y;
		}
	}

	/* reset y by shifting it back down */
	rshift_digits(&y, n - t);

	/* step 3. for i from n down to (t + 1) */
	for (i = n; i >= (t + 1); i--) {
		if (i > x.used) {
			continue;
		}

		/* step 3.1 if xi == yt then set q{i-t-1} to b-1, 
		* otherwise set q{i-t-1} to (xi*b + x{i-1})/yt */
		if (x.dp[i] == y.dp[t]) {
			q.dp[i - t - 1] = ((((mp_digit)1) << DIGIT_BIT) - 1);
		} else {
			mp_word tmp;
			tmp = ((mp_word) x.dp[i]) << ((mp_word) DIGIT_BIT);
			tmp |= ((mp_word) x.dp[i - 1]);
			tmp /= ((mp_word) y.dp[t]);
			if (tmp > (mp_word) MP_MASK) {
				tmp = MP_MASK;
			}
			q.dp[i - t - 1] = (mp_digit) (tmp & (mp_word) (MP_MASK));
		}

		/* while (q{i-t-1} * (yt * b + y{t-1})) > 
		     xi * b**2 + xi-1 * b + xi-2 
			do q{i-t-1} -= 1; 
		*/
		q.dp[i - t - 1] = (q.dp[i - t - 1] + 1) & MP_MASK;
		do {
			q.dp[i - t - 1] = (q.dp[i - t - 1] - 1) & MP_MASK;

			/* find left hand */
			mp_zero(&t1);
			t1.dp[0] = (t - 1 < 0) ? 0 : y.dp[t - 1];
			t1.dp[1] = y.dp[t];
			t1.used = 2;
			if ((res = multiply_digit(&t1, q.dp[i - t - 1], &t1)) != MP_OKAY) {
				goto LBL_Y;
			}

			/* find right hand */
			t2.dp[0] = (i - 2 < 0) ? 0 : x.dp[i - 2];
			t2.dp[1] = (i - 1 < 0) ? 0 : x.dp[i - 1];
			t2.dp[2] = x.dp[i];
			t2.used = 3;
		} while (compare_magnitude(&t1, &t2) == MP_GT);

		/* step 3.3 x = x - q{i-t-1} * y * b**{i-t-1} */
		if ((res = multiply_digit(&y, q.dp[i - t - 1], &t1)) != MP_OKAY) {
			goto LBL_Y;
		}

		if ((res = lshift_digits(&t1, i - t - 1)) != MP_OKAY) {
			goto LBL_Y;
		}

		if ((res = signed_subtract(&x, &t1, &x)) != MP_OKAY) {
			goto LBL_Y;
		}

		/* if x < 0 then { x = x + y*b**{i-t-1}; q{i-t-1} -= 1; } */
		if (x.sign == MP_NEG) {
			if ((res = mp_copy(&y, &t1)) != MP_OKAY) {
				goto LBL_Y;
			}
			if ((res = lshift_digits(&t1, i - t - 1)) != MP_OKAY) {
				goto LBL_Y;
			}
			if ((res = signed_add(&x, &t1, &x)) != MP_OKAY) {
				goto LBL_Y;
			}

			q.dp[i - t - 1] = (q.dp[i - t - 1] - 1UL) & MP_MASK;
		}
	}

	/* now q is the quotient and x is the remainder 
	* [which we have to normalize] 
	*/

	/* get sign before writing to c */
	x.sign = x.used == 0 ? MP_ZPOS : a->sign;

	if (c != NULL) {
		trim_unused_digits(&q);
		mp_exch(&q, c);
		c->sign = neg;
	}

	if (d != NULL) {
		rshift_bits(&x, norm, &x, NULL);
		mp_exch(&x, d);
	}

	res = MP_OKAY;

LBL_Y:
	mp_clear(&y);
LBL_X:
	mp_clear(&x);
LBL_T2:
	mp_clear(&t2);
LBL_T1:
	mp_clear(&t1);
LBL_Q:
	mp_clear(&q);
	return res;
}

/* c = a mod b, 0 <= c < b */
static int
modulo(mp_int * a, mp_int * b, mp_int * c)
{
	mp_int  t;
	int     res;

	if ((res = mp_init(&t)) != MP_OKAY) {
		return res;
	}

	if ((res = signed_divide(NULL, &t, a, b)) != MP_OKAY) {
		mp_clear(&t);
		return res;
	}

	if (t.sign != b->sign) {
		res = signed_add(b, &t, c);
	} else {
		res = MP_OKAY;
		mp_exch(&t, c);
	}

	mp_clear(&t);
	return res;
}

/* set to a digit */
static void
set_word(mp_int * a, mp_digit b)
{
	mp_zero(a);
	a->dp[0] = b & MP_MASK;
	a->used = (a->dp[0] != 0) ? 1 : 0;
}

/* b = a/2 */
static int
half(mp_int * a, mp_int * b)
{
	int     x, res, oldused;

	/* copy */
	if (b->alloc < a->used) {
		if ((res = mp_grow(b, a->used)) != MP_OKAY) {
			return res;
		}
	}

	oldused = b->used;
	b->used = a->used;
	{
		mp_digit r, rr, *tmpa, *tmpb;

		/* source alias */
		tmpa = a->dp + b->used - 1;

		/* dest alias */
		tmpb = b->dp + b->used - 1;

		/* carry */
		r = 0;
		for (x = b->used - 1; x >= 0; x--) {
			/* get the carry for the next iteration */
			rr = *tmpa & 1;

			/* shift the current digit, add in carry and store */
			*tmpb-- = (*tmpa-- >> 1) | (r << (DIGIT_BIT - 1));

			/* forward carry to next iteration */
			r = rr;
		}

		/* zero excess digits */
		tmpb = b->dp + b->used;
		for (x = b->used; x < oldused; x++) {
			*tmpb++ = 0;
		}
	}
	b->sign = a->sign;
	trim_unused_digits(b);
	return MP_OKAY;
}

/* compare a digit */
static int
compare_digit(mp_int * a, mp_digit b)
{
	/* compare based on sign */
	if (a->sign == MP_NEG) {
		return MP_LT;
	}

	/* compare based on magnitude */
	if (a->used > 1) {
		return MP_GT;
	}

	/* compare the only digit of a to b */
	if (a->dp[0] > b) {
		return MP_GT;
	} else if (a->dp[0] < b) {
		return MP_LT;
	} else {
		return MP_EQ;
	}
}

static void
mp_clear_multi(mp_int *mp, ...) 
{
	mp_int* next_mp = mp;
	va_list args;

	va_start(args, mp);
	while (next_mp != NULL) {
		mp_clear(next_mp);
		next_mp = va_arg(args, mp_int*);
	}
	va_end(args);
}

/* computes the modular inverse via binary extended euclidean algorithm, 
 * that is c = 1/a mod b 
 *
 * Based on slow invmod except this is optimized for the case where b is 
 * odd as per HAC Note 14.64 on pp. 610
 */
static int
fast_modular_inverse(mp_int * a, mp_int * b, mp_int * c)
{
	mp_int  x, y, u, v, B, D;
	int     res, neg;

	/* 2. [modified] b must be odd   */
	if (MP_ISZERO(b) == MP_YES) {
		return MP_VAL;
	}

	/* init all our temps */
	if ((res = mp_init_multi(&x, &y, &u, &v, &B, &D, NULL)) != MP_OKAY) {
		return res;
	}

	/* x == modulus, y == value to invert */
	if ((res = mp_copy(b, &x)) != MP_OKAY) {
		goto LBL_ERR;
	}

	/* we need y = |a| */
	if ((res = modulo(a, b, &y)) != MP_OKAY) {
		goto LBL_ERR;
	}

	/* 3. u=x, v=y, A=1, B=0, C=0,D=1 */
	if ((res = mp_copy(&x, &u)) != MP_OKAY) {
		goto LBL_ERR;
	}
	if ((res = mp_copy(&y, &v)) != MP_OKAY) {
		goto LBL_ERR;
	}
	set_word(&D, 1);

top:
	/* 4.  while u is even do */
	while (BN_is_even(&u) == 1) {
		/* 4.1 u = u/2 */
		if ((res = half(&u, &u)) != MP_OKAY) {
			goto LBL_ERR;
		}
		/* 4.2 if B is odd then */
		if (BN_is_odd(&B) == 1) {
			if ((res = signed_subtract(&B, &x, &B)) != MP_OKAY) {
				goto LBL_ERR;
			}
		}
		/* B = B/2 */
		if ((res = half(&B, &B)) != MP_OKAY) {
			goto LBL_ERR;
		}
	}

	/* 5.  while v is even do */
	while (BN_is_even(&v) == 1) {
		/* 5.1 v = v/2 */
		if ((res = half(&v, &v)) != MP_OKAY) {
			goto LBL_ERR;
		}
		/* 5.2 if D is odd then */
		if (BN_is_odd(&D) == 1) {
			/* D = (D-x)/2 */
			if ((res = signed_subtract(&D, &x, &D)) != MP_OKAY) {
				goto LBL_ERR;
			}
		}
		/* D = D/2 */
		if ((res = half(&D, &D)) != MP_OKAY) {
			goto LBL_ERR;
		}
	}

	/* 6.  if u >= v then */
	if (signed_compare(&u, &v) != MP_LT) {
		/* u = u - v, B = B - D */
		if ((res = signed_subtract(&u, &v, &u)) != MP_OKAY) {
			goto LBL_ERR;
		}

		if ((res = signed_subtract(&B, &D, &B)) != MP_OKAY) {
			goto LBL_ERR;
		}
	} else {
		/* v - v - u, D = D - B */
		if ((res = signed_subtract(&v, &u, &v)) != MP_OKAY) {
			goto LBL_ERR;
		}

		if ((res = signed_subtract(&D, &B, &D)) != MP_OKAY) {
			goto LBL_ERR;
		}
	}

	/* if not zero goto step 4 */
	if (MP_ISZERO(&u) == MP_NO) {
		goto top;
	}

	/* now a = C, b = D, gcd == g*v */

	/* if v != 1 then there is no inverse */
	if (compare_digit(&v, 1) != MP_EQ) {
		res = MP_VAL;
		goto LBL_ERR;
	}

	/* b is now the inverse */
	neg = a->sign;
	while (D.sign == MP_NEG) {
		if ((res = signed_add(&D, b, &D)) != MP_OKAY) {
			goto LBL_ERR;
		}
	}
	mp_exch(&D, c);
	c->sign = neg;
	res = MP_OKAY;

LBL_ERR:
	mp_clear_multi (&x, &y, &u, &v, &B, &D, NULL);
	return res;
}

/* hac 14.61, pp608 */
static int
slow_modular_inverse(mp_int * a, mp_int * b, mp_int * c)
{
	mp_int  x, y, u, v, A, B, C, D;
	int     res;

	/* b cannot be negative */
	if (b->sign == MP_NEG || MP_ISZERO(b) == MP_YES) {
		return MP_VAL;
	}

	/* init temps */
	if ((res = mp_init_multi(&x, &y, &u, &v, 
		   &A, &B, &C, &D, NULL)) != MP_OKAY) {
		return res;
	}

	/* x = a, y = b */
	if ((res = modulo(a, b, &x)) != MP_OKAY) {
		goto LBL_ERR;
	}
	if ((res = mp_copy(b, &y)) != MP_OKAY) {
		goto LBL_ERR;
	}

	/* 2. [modified] if x,y are both even then return an error! */
	if (BN_is_even(&x) == 1 && BN_is_even(&y) == 1) {
		res = MP_VAL;
		goto LBL_ERR;
	}

	/* 3. u=x, v=y, A=1, B=0, C=0,D=1 */
	if ((res = mp_copy(&x, &u)) != MP_OKAY) {
		goto LBL_ERR;
	}
	if ((res = mp_copy(&y, &v)) != MP_OKAY) {
		goto LBL_ERR;
	}
	set_word(&A, 1);
	set_word(&D, 1);

top:
	/* 4.  while u is even do */
	while (BN_is_even(&u) == 1) {
		/* 4.1 u = u/2 */
		if ((res = half(&u, &u)) != MP_OKAY) {
			goto LBL_ERR;
		}
		/* 4.2 if A or B is odd then */
		if (BN_is_odd(&A) == 1 || BN_is_odd(&B) == 1) {
			/* A = (A+y)/2, B = (B-x)/2 */
			if ((res = signed_add(&A, &y, &A)) != MP_OKAY) {
				 goto LBL_ERR;
			}
			if ((res = signed_subtract(&B, &x, &B)) != MP_OKAY) {
				 goto LBL_ERR;
			}
		}
		/* A = A/2, B = B/2 */
		if ((res = half(&A, &A)) != MP_OKAY) {
			goto LBL_ERR;
		}
		if ((res = half(&B, &B)) != MP_OKAY) {
			goto LBL_ERR;
		}
	}

	/* 5.  while v is even do */
	while (BN_is_even(&v) == 1) {
		/* 5.1 v = v/2 */
		if ((res = half(&v, &v)) != MP_OKAY) {
			goto LBL_ERR;
		}
		/* 5.2 if C or D is odd then */
		if (BN_is_odd(&C) == 1 || BN_is_odd(&D) == 1) {
			/* C = (C+y)/2, D = (D-x)/2 */
			if ((res = signed_add(&C, &y, &C)) != MP_OKAY) {
				 goto LBL_ERR;
			}
			if ((res = signed_subtract(&D, &x, &D)) != MP_OKAY) {
				 goto LBL_ERR;
			}
		}
		/* C = C/2, D = D/2 */
		if ((res = half(&C, &C)) != MP_OKAY) {
			goto LBL_ERR;
		}
		if ((res = half(&D, &D)) != MP_OKAY) {
			goto LBL_ERR;
		}
	}

	/* 6.  if u >= v then */
	if (signed_compare(&u, &v) != MP_LT) {
		/* u = u - v, A = A - C, B = B - D */
		if ((res = signed_subtract(&u, &v, &u)) != MP_OKAY) {
			goto LBL_ERR;
		}

		if ((res = signed_subtract(&A, &C, &A)) != MP_OKAY) {
			goto LBL_ERR;
		}

		if ((res = signed_subtract(&B, &D, &B)) != MP_OKAY) {
			goto LBL_ERR;
		}
	} else {
		/* v - v - u, C = C - A, D = D - B */
		if ((res = signed_subtract(&v, &u, &v)) != MP_OKAY) {
			goto LBL_ERR;
		}

		if ((res = signed_subtract(&C, &A, &C)) != MP_OKAY) {
			goto LBL_ERR;
		}

		if ((res = signed_subtract(&D, &B, &D)) != MP_OKAY) {
			goto LBL_ERR;
		}
	}

	/* if not zero goto step 4 */
	if (BN_is_zero(&u) == 0) {
		goto top;
	}
	/* now a = C, b = D, gcd == g*v */

	/* if v != 1 then there is no inverse */
	if (compare_digit(&v, 1) != MP_EQ) {
		res = MP_VAL;
		goto LBL_ERR;
	}

	/* if its too low */
	while (compare_digit(&C, 0) == MP_LT) {
		if ((res = signed_add(&C, b, &C)) != MP_OKAY) {
			 goto LBL_ERR;
		}
	}

	/* too big */
	while (compare_magnitude(&C, b) != MP_LT) {
		if ((res = signed_subtract(&C, b, &C)) != MP_OKAY) {
			 goto LBL_ERR;
		}
	}

	/* C is now the inverse */
	mp_exch(&C, c);
	res = MP_OKAY;
LBL_ERR:
	mp_clear_multi(&x, &y, &u, &v, &A, &B, &C, &D, NULL);
	return res;
}

static int
modular_inverse(mp_int *c, mp_int *a, mp_int *b)
{
	/* b cannot be negative */
	if (b->sign == MP_NEG || MP_ISZERO(b) == MP_YES) {
		return MP_VAL;
	}

	/* if the modulus is odd we can use a faster routine instead */
	if (BN_is_odd(b) == 1) {
		return fast_modular_inverse(a, b, c);
	}
	return slow_modular_inverse(a, b, c);
}

/* b = |a| 
 *
 * Simple function copies the input and fixes the sign to positive
 */
static int
absolute(mp_int * a, mp_int * b)
{
	int     res;

	/* copy a to b */
	if (a != b) {
		if ((res = mp_copy(a, b)) != MP_OKAY) {
			return res;
		}
	}

	/* force the sign of b to positive */
	b->sign = MP_ZPOS;

	return MP_OKAY;
}

/* determines if reduce_2k_l can be used */
static int
mp_reduce_is_2k_l(mp_int *a)
{
	int ix, iy;

	if (a->used == 0) {
		return MP_NO;
	} else if (a->used == 1) {
		return MP_YES;
	} else if (a->used > 1) {
		/* if more than half of the digits are -1 we're sold */
		for (iy = ix = 0; ix < a->used; ix++) {
			if (a->dp[ix] == MP_MASK) {
				++iy;
			}
		}
		return (iy >= (a->used/2)) ? MP_YES : MP_NO;

	}
	return MP_NO;
}

/* computes a = 2**b 
 *
 * Simple algorithm which zeroes the int, grows it then just sets one bit
 * as required.
 */
static int
mp_2expt(mp_int * a, int b)
{
	int     res;

	/* zero a as per default */
	mp_zero(a);

	/* grow a to accomodate the single bit */
	if ((res = mp_grow(a, b / DIGIT_BIT + 1)) != MP_OKAY) {
		return res;
	}

	/* set the used count of where the bit will go */
	a->used = b / DIGIT_BIT + 1;

	/* put the single bit in its place */
	a->dp[b / DIGIT_BIT] = ((mp_digit)1) << (b % DIGIT_BIT);

	return MP_OKAY;
}

/* pre-calculate the value required for Barrett reduction
 * For a given modulus "b" it calulates the value required in "a"
 */
static int
mp_reduce_setup(mp_int * a, mp_int * b)
{
	int     res;

	if ((res = mp_2expt(a, b->used * 2 * DIGIT_BIT)) != MP_OKAY) {
		return res;
	}
	return signed_divide(a, NULL, a, b);
}

/* b = a*2 */
static int
doubled(mp_int * a, mp_int * b)
{
	int     x, res, oldused;

	/* grow to accomodate result */
	if (b->alloc < a->used + 1) {
		if ((res = mp_grow(b, a->used + 1)) != MP_OKAY) {
			return res;
		}
	}

	oldused = b->used;
	b->used = a->used;

	{
		mp_digit r, rr, *tmpa, *tmpb;

		/* alias for source */
		tmpa = a->dp;

		/* alias for dest */
		tmpb = b->dp;

		/* carry */
		r = 0;
		for (x = 0; x < a->used; x++) {

			/* get what will be the *next* carry bit from the 
			* MSB of the current digit 
			*/
			rr = *tmpa >> ((mp_digit)(DIGIT_BIT - 1));

			/* now shift up this digit, add in the carry [from the previous] */
			*tmpb++ = ((*tmpa++ << ((mp_digit)1)) | r) & MP_MASK;

			/* copy the carry that would be from the source 
			* digit into the next iteration 
			*/
			r = rr;
		}

		/* new leading digit? */
		if (r != 0) {
			/* add a MSB which is always 1 at this point */
			*tmpb = 1;
			++(b->used);
		}

		/* now zero any excess digits on the destination 
		* that we didn't write to 
		*/
		tmpb = b->dp + b->used;
		for (x = b->used; x < oldused; x++) {
			*tmpb++ = 0;
		}
	}
	b->sign = a->sign;
	return MP_OKAY;
}

/* divide by three (based on routine from MPI and the GMP manual) */
static int
third(mp_int * a, mp_int *c, mp_digit * d)
{
	mp_int   q;
	mp_word  w, t;
	mp_digit b;
	int      res, ix;

	/* b = 2**DIGIT_BIT / 3 */
	b = (((mp_word)1) << ((mp_word)DIGIT_BIT)) / ((mp_word)3);

	if ((res = mp_init_size(&q, a->used)) != MP_OKAY) {
		return res;
	}

	q.used = a->used;
	q.sign = a->sign;
	w = 0;
	for (ix = a->used - 1; ix >= 0; ix--) {
		w = (w << ((mp_word)DIGIT_BIT)) | ((mp_word)a->dp[ix]);

		if (w >= 3) {
			/* multiply w by [1/3] */
			t = (w * ((mp_word)b)) >> ((mp_word)DIGIT_BIT);

			/* now subtract 3 * [w/3] from w, to get the remainder */
			w -= t+t+t;

			/* fixup the remainder as required since
			* the optimization is not exact.
			*/
			while (w >= 3) {
				t += 1;
				w -= 3;
			}
		} else {
			t = 0;
		}
		q.dp[ix] = (mp_digit)t;
	}

	/* [optional] store the remainder */
	if (d != NULL) {
		*d = (mp_digit)w;
	}

	/* [optional] store the quotient */
	if (c != NULL) {
		trim_unused_digits(&q);
		mp_exch(&q, c);
	}
	mp_clear(&q);

	return res;
}

/* multiplication using the Toom-Cook 3-way algorithm 
 *
 * Much more complicated than Karatsuba but has a lower 
 * asymptotic running time of O(N**1.464).  This algorithm is 
 * only particularly useful on VERY large inputs 
 * (we're talking 1000s of digits here...).
*/
static int
toom_cook_multiply(mp_int *a, mp_int *b, mp_int *c)
{
	mp_int w0, w1, w2, w3, w4, tmp1, tmp2, a0, a1, a2, b0, b1, b2;
	int res, B;

	/* init temps */
	if ((res = mp_init_multi(&w0, &w1, &w2, &w3, &w4, 
			&a0, &a1, &a2, &b0, &b1, 
			&b2, &tmp1, &tmp2, NULL)) != MP_OKAY) {
		return res;
	}

	/* B */
	B = MIN(a->used, b->used) / 3;

	/* a = a2 * B**2 + a1 * B + a0 */
	if ((res = modulo_2_to_power(a, DIGIT_BIT * B, &a0)) != MP_OKAY) {
		goto ERR;
	}

	if ((res = mp_copy(a, &a1)) != MP_OKAY) {
		goto ERR;
	}
	rshift_digits(&a1, B);
	modulo_2_to_power(&a1, DIGIT_BIT * B, &a1);

	if ((res = mp_copy(a, &a2)) != MP_OKAY) {
		goto ERR;
	}
	rshift_digits(&a2, B*2);

	/* b = b2 * B**2 + b1 * B + b0 */
	if ((res = modulo_2_to_power(b, DIGIT_BIT * B, &b0)) != MP_OKAY) {
		goto ERR;
	}

	if ((res = mp_copy(b, &b1)) != MP_OKAY) {
		goto ERR;
	}
	rshift_digits(&b1, B);
	modulo_2_to_power(&b1, DIGIT_BIT * B, &b1);

	if ((res = mp_copy(b, &b2)) != MP_OKAY) {
		goto ERR;
	}
	rshift_digits(&b2, B*2);

	/* w0 = a0*b0 */
	if ((res = signed_multiply(&a0, &b0, &w0)) != MP_OKAY) {
		goto ERR;
	}

	/* w4 = a2 * b2 */
	if ((res = signed_multiply(&a2, &b2, &w4)) != MP_OKAY) {
		goto ERR;
	}

	/* w1 = (a2 + 2(a1 + 2a0))(b2 + 2(b1 + 2b0)) */
	if ((res = doubled(&a0, &tmp1)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = signed_add(&tmp1, &a1, &tmp1)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = doubled(&tmp1, &tmp1)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = signed_add(&tmp1, &a2, &tmp1)) != MP_OKAY) {
		goto ERR;
	}

	if ((res = doubled(&b0, &tmp2)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = signed_add(&tmp2, &b1, &tmp2)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = doubled(&tmp2, &tmp2)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = signed_add(&tmp2, &b2, &tmp2)) != MP_OKAY) {
		goto ERR;
	}

	if ((res = signed_multiply(&tmp1, &tmp2, &w1)) != MP_OKAY) {
		goto ERR;
	}

	/* w3 = (a0 + 2(a1 + 2a2))(b0 + 2(b1 + 2b2)) */
	if ((res = doubled(&a2, &tmp1)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = signed_add(&tmp1, &a1, &tmp1)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = doubled(&tmp1, &tmp1)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = signed_add(&tmp1, &a0, &tmp1)) != MP_OKAY) {
		goto ERR;
	}

	if ((res = doubled(&b2, &tmp2)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = signed_add(&tmp2, &b1, &tmp2)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = doubled(&tmp2, &tmp2)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = signed_add(&tmp2, &b0, &tmp2)) != MP_OKAY) {
		goto ERR;
	}

	if ((res = signed_multiply(&tmp1, &tmp2, &w3)) != MP_OKAY) {
		goto ERR;
	}


	/* w2 = (a2 + a1 + a0)(b2 + b1 + b0) */
	if ((res = signed_add(&a2, &a1, &tmp1)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = signed_add(&tmp1, &a0, &tmp1)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = signed_add(&b2, &b1, &tmp2)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = signed_add(&tmp2, &b0, &tmp2)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = signed_multiply(&tmp1, &tmp2, &w2)) != MP_OKAY) {
		goto ERR;
	}

	/* now solve the matrix 

	0  0  0  0  1
	1  2  4  8  16
	1  1  1  1  1
	16 8  4  2  1
	1  0  0  0  0

	using 12 subtractions, 4 shifts, 
	2 small divisions and 1 small multiplication 
	*/

	/* r1 - r4 */
	if ((res = signed_subtract(&w1, &w4, &w1)) != MP_OKAY) {
		goto ERR;
	}
	/* r3 - r0 */
	if ((res = signed_subtract(&w3, &w0, &w3)) != MP_OKAY) {
		goto ERR;
	}
	/* r1/2 */
	if ((res = half(&w1, &w1)) != MP_OKAY) {
		goto ERR;
	}
	/* r3/2 */
	if ((res = half(&w3, &w3)) != MP_OKAY) {
		goto ERR;
	}
	/* r2 - r0 - r4 */
	if ((res = signed_subtract(&w2, &w0, &w2)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = signed_subtract(&w2, &w4, &w2)) != MP_OKAY) {
		goto ERR;
	}
	/* r1 - r2 */
	if ((res = signed_subtract(&w1, &w2, &w1)) != MP_OKAY) {
		goto ERR;
	}
	/* r3 - r2 */
	if ((res = signed_subtract(&w3, &w2, &w3)) != MP_OKAY) {
		goto ERR;
	}
	/* r1 - 8r0 */
	if ((res = lshift_bits(&w0, 3, &tmp1)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = signed_subtract(&w1, &tmp1, &w1)) != MP_OKAY) {
		goto ERR;
	}
	/* r3 - 8r4 */
	if ((res = lshift_bits(&w4, 3, &tmp1)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = signed_subtract(&w3, &tmp1, &w3)) != MP_OKAY) {
		goto ERR;
	}
	/* 3r2 - r1 - r3 */
	if ((res = multiply_digit(&w2, 3, &w2)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = signed_subtract(&w2, &w1, &w2)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = signed_subtract(&w2, &w3, &w2)) != MP_OKAY) {
		goto ERR;
	}
	/* r1 - r2 */
	if ((res = signed_subtract(&w1, &w2, &w1)) != MP_OKAY) {
		goto ERR;
	}
	/* r3 - r2 */
	if ((res = signed_subtract(&w3, &w2, &w3)) != MP_OKAY) {
		goto ERR;
	}
	/* r1/3 */
	if ((res = third(&w1, &w1, NULL)) != MP_OKAY) {
		goto ERR;
	}
	/* r3/3 */
	if ((res = third(&w3, &w3, NULL)) != MP_OKAY) {
		goto ERR;
	}

	/* at this point shift W[n] by B*n */
	if ((res = lshift_digits(&w1, 1*B)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = lshift_digits(&w2, 2*B)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = lshift_digits(&w3, 3*B)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = lshift_digits(&w4, 4*B)) != MP_OKAY) {
		goto ERR;
	}     

	if ((res = signed_add(&w0, &w1, c)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = signed_add(&w2, &w3, &tmp1)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = signed_add(&w4, &tmp1, &tmp1)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = signed_add(&tmp1, c, c)) != MP_OKAY) {
		goto ERR;
	}     

ERR:
	mp_clear_multi(&w0, &w1, &w2, &w3, &w4, 
		&a0, &a1, &a2, &b0, &b1, 
		&b2, &tmp1, &tmp2, NULL);
	return res;
}     
     
#define TOOM_MUL_CUTOFF	350
#define KARATSUBA_MUL_CUTOFF 80

/* c = |a| * |b| using Karatsuba Multiplication using 
 * three half size multiplications
 *
 * Let B represent the radix [e.g. 2**DIGIT_BIT] and 
 * let n represent half of the number of digits in 
 * the min(a,b)
 *
 * a = a1 * B**n + a0
 * b = b1 * B**n + b0
 *
 * Then, a * b => 
   a1b1 * B**2n + ((a1 + a0)(b1 + b0) - (a0b0 + a1b1)) * B + a0b0
 *
 * Note that a1b1 and a0b0 are used twice and only need to be 
 * computed once.  So in total three half size (half # of 
 * digit) multiplications are performed, a0b0, a1b1 and 
 * (a1+b1)(a0+b0)
 *
 * Note that a multiplication of half the digits requires
 * 1/4th the number of single precision multiplications so in 
 * total after one call 25% of the single precision multiplications 
 * are saved.  Note also that the call to signed_multiply can end up back 
 * in this function if the a0, a1, b0, or b1 are above the threshold.  
 * This is known as divide-and-conquer and leads to the famous 
 * O(N**lg(3)) or O(N**1.584) work which is asymptopically lower than 
 * the standard O(N**2) that the baseline/comba methods use.  
 * Generally though the overhead of this method doesn't pay off 
 * until a certain size (N ~ 80) is reached.
 */
static int
karatsuba_multiply(mp_int * a, mp_int * b, mp_int * c)
{
	mp_int  x0, x1, y0, y1, t1, x0y0, x1y1;
	int     B;
	int     err;

	/* default the return code to an error */
	err = MP_MEM;

	/* min # of digits */
	B = MIN(a->used, b->used);

	/* now divide in two */
	B = (int)((unsigned)B >> 1);

	/* init copy all the temps */
	if (mp_init_size(&x0, B) != MP_OKAY) {
		goto ERR;
	}
	if (mp_init_size(&x1, a->used - B) != MP_OKAY) {
		goto X0;
	}
	if (mp_init_size(&y0, B) != MP_OKAY) {
		goto X1;
	}
	if (mp_init_size(&y1, b->used - B) != MP_OKAY) {
		goto Y0;
	}
	/* init temps */
	if (mp_init_size(&t1, B * 2) != MP_OKAY) {
		goto Y1;
	}
	if (mp_init_size(&x0y0, B * 2) != MP_OKAY) {
		goto T1;
	}
	if (mp_init_size(&x1y1, B * 2) != MP_OKAY) {
		goto X0Y0;
	}
	/* now shift the digits */
	x0.used = y0.used = B;
	x1.used = a->used - B;
	y1.used = b->used - B;

	{
		int x;
		mp_digit *tmpa, *tmpb, *tmpx, *tmpy;

		/* we copy the digits directly instead of using higher level functions
		* since we also need to shift the digits
		*/
		tmpa = a->dp;
		tmpb = b->dp;

		tmpx = x0.dp;
		tmpy = y0.dp;
		for (x = 0; x < B; x++) {
			*tmpx++ = *tmpa++;
			*tmpy++ = *tmpb++;
		}

		tmpx = x1.dp;
		for (x = B; x < a->used; x++) {
			*tmpx++ = *tmpa++;
		}

		tmpy = y1.dp;
		for (x = B; x < b->used; x++) {
			*tmpy++ = *tmpb++;
		}
	}

	/* only need to clamp the lower words since by definition the 
	* upper words x1/y1 must have a known number of digits
	*/
	trim_unused_digits(&x0);
	trim_unused_digits(&y0);

	/* now calc the products x0y0 and x1y1 */
	/* after this x0 is no longer required, free temp [x0==t2]! */
	if (signed_multiply(&x0, &y0, &x0y0) != MP_OKAY)  {
		goto X1Y1;          /* x0y0 = x0*y0 */
	}
	if (signed_multiply(&x1, &y1, &x1y1) != MP_OKAY) {
		goto X1Y1;          /* x1y1 = x1*y1 */
	}
	/* now calc x1+x0 and y1+y0 */
	if (basic_add(&x1, &x0, &t1) != MP_OKAY) {
		goto X1Y1;          /* t1 = x1 - x0 */
	}
	if (basic_add(&y1, &y0, &x0) != MP_OKAY) {
		goto X1Y1;          /* t2 = y1 - y0 */
	}
	if (signed_multiply(&t1, &x0, &t1) != MP_OKAY) {
		goto X1Y1;          /* t1 = (x1 + x0) * (y1 + y0) */
	}
	/* add x0y0 */
	if (signed_add(&x0y0, &x1y1, &x0) != MP_OKAY) {
		goto X1Y1;          /* t2 = x0y0 + x1y1 */
	}
	if (basic_subtract(&t1, &x0, &t1) != MP_OKAY) {
		goto X1Y1;          /* t1 = (x1+x0)*(y1+y0) - (x1y1 + x0y0) */
	}
	/* shift by B */
	if (lshift_digits(&t1, B) != MP_OKAY) {
		goto X1Y1;          /* t1 = (x0y0 + x1y1 - (x1-x0)*(y1-y0))<<B */
	}
	if (lshift_digits(&x1y1, B * 2) != MP_OKAY) {
		goto X1Y1;          /* x1y1 = x1y1 << 2*B */
	}
	if (signed_add(&x0y0, &t1, &t1) != MP_OKAY) {
		goto X1Y1;          /* t1 = x0y0 + t1 */
	}
	if (signed_add(&t1, &x1y1, c) != MP_OKAY) {
		goto X1Y1;          /* t1 = x0y0 + t1 + x1y1 */
	}
	/* Algorithm succeeded set the return code to MP_OKAY */
	err = MP_OKAY;

X1Y1:
	mp_clear(&x1y1);
X0Y0:
	mp_clear(&x0y0);
T1:
	mp_clear(&t1);
Y1:
	mp_clear(&y1);
Y0:
	mp_clear(&y0);
X1:
	mp_clear(&x1);
X0:
	mp_clear(&x0);
ERR:
	return err;
}

/* Fast (comba) multiplier
 *
 * This is the fast column-array [comba] multiplier.  It is 
 * designed to compute the columns of the product first 
 * then handle the carries afterwards.  This has the effect 
 * of making the nested loops that compute the columns very
 * simple and schedulable on super-scalar processors.
 *
 * This has been modified to produce a variable number of 
 * digits of output so if say only a half-product is required 
 * you don't have to compute the upper half (a feature 
 * required for fast Barrett reduction).
 *
 * Based on Algorithm 14.12 on pp.595 of HAC.
 *
 */
static int
fast_col_array_multiply(mp_int * a, mp_int * b, mp_int * c, int digs)
{
	int     olduse, res, pa, ix, iz;
	/*LINTED*/
	mp_digit W[MP_WARRAY];
	mp_word  _W;

	/* grow the destination as required */
	if (c->alloc < digs) {
		if ((res = mp_grow(c, digs)) != MP_OKAY) {
			return res;
		}
	}

	/* number of output digits to produce */
	pa = MIN(digs, a->used + b->used);

	/* clear the carry */
	_W = 0;
	for (ix = 0; ix < pa; ix++) { 
		int      tx, ty;
		int      iy;
		mp_digit *tmpx, *tmpy;

		/* get offsets into the two bignums */
		ty = MIN(b->used-1, ix);
		tx = ix - ty;

		/* setup temp aliases */
		tmpx = a->dp + tx;
		tmpy = b->dp + ty;

		/* this is the number of times the loop will iterrate, essentially 
		while (tx++ < a->used && ty-- >= 0) { ... }
		*/
		iy = MIN(a->used-tx, ty+1);

		/* execute loop */
		for (iz = 0; iz < iy; ++iz) {
			_W += ((mp_word)*tmpx++)*((mp_word)*tmpy--);

		}

		/* store term */
		W[ix] = ((mp_digit)_W) & MP_MASK;

		/* make next carry */
		_W = _W >> ((mp_word)DIGIT_BIT);
	}

	/* setup dest */
	olduse  = c->used;
	c->used = pa;

	{
		mp_digit *tmpc;
		tmpc = c->dp;
		for (ix = 0; ix < pa+1; ix++) {
			/* now extract the previous digit [below the carry] */
			*tmpc++ = W[ix];
		}

		/* clear unused digits [that existed in the old copy of c] */
		for (; ix < olduse; ix++) {
			*tmpc++ = 0;
		}
	}
	trim_unused_digits(c);
	return MP_OKAY;
}

/* return 1 if we can use fast column array multiply */
/*
* The fast multiplier can be used if the output will 
* have less than MP_WARRAY digits and the number of 
* digits won't affect carry propagation
*/
static inline int
can_use_fast_column_array(int ndigits, int used)
{
	return (((unsigned)ndigits < MP_WARRAY) &&
		used < (1 << (unsigned)((CHAR_BIT * sizeof(mp_word)) - (2 * DIGIT_BIT))));
}

/* Source: /usr/cvsroot/libtommath/dist/libtommath/bn_fast_s_mp_mul_digs.c,v $ */
/* Revision: 1.2 $ */
/* Date: 2011/03/18 16:22:09 $ */


/* multiplies |a| * |b| and only computes upto digs digits of result
 * HAC pp. 595, Algorithm 14.12  Modified so you can control how 
 * many digits of output are created.
 */
static int
basic_multiply_partial_lower(mp_int * a, mp_int * b, mp_int * c, int digs)
{
	mp_int  t;
	int     res, pa, pb, ix, iy;
	mp_digit u;
	mp_word r;
	mp_digit tmpx, *tmpt, *tmpy;

	/* can we use the fast multiplier? */
	if (can_use_fast_column_array(digs, MIN(a->used, b->used))) {
		return fast_col_array_multiply(a, b, c, digs);
	}

	if ((res = mp_init_size(&t, digs)) != MP_OKAY) {
		return res;
	}
	t.used = digs;

	/* compute the digits of the product directly */
	pa = a->used;
	for (ix = 0; ix < pa; ix++) {
		/* set the carry to zero */
		u = 0;

		/* limit ourselves to making digs digits of output */
		pb = MIN(b->used, digs - ix);

		/* setup some aliases */
		/* copy of the digit from a used within the nested loop */
		tmpx = a->dp[ix];

		/* an alias for the destination shifted ix places */
		tmpt = t.dp + ix;

		/* an alias for the digits of b */
		tmpy = b->dp;

		/* compute the columns of the output and propagate the carry */
		for (iy = 0; iy < pb; iy++) {
			/* compute the column as a mp_word */
			r = ((mp_word)*tmpt) +
				((mp_word)tmpx) * ((mp_word)*tmpy++) +
				((mp_word) u);

			/* the new column is the lower part of the result */
			*tmpt++ = (mp_digit) (r & ((mp_word) MP_MASK));

			/* get the carry word from the result */
			u = (mp_digit) (r >> ((mp_word) DIGIT_BIT));
		}
		/* set carry if it is placed below digs */
		if (ix + iy < digs) {
			*tmpt = u;
		}
	}

	trim_unused_digits(&t);
	mp_exch(&t, c);

	mp_clear(&t);
	return MP_OKAY;
}

/* Source: /usr/cvsroot/libtommath/dist/libtommath/bn_s_mp_mul_digs.c,v $ */
/* Revision: 1.3 $ */
/* Date: 2011/03/18 16:43:04 $ */

/* high level multiplication (handles sign) */
static int
signed_multiply(mp_int * a, mp_int * b, mp_int * c)
{
	int     res, neg;

	neg = (a->sign == b->sign) ? MP_ZPOS : MP_NEG;
	/* use Toom-Cook? */
	if (MIN(a->used, b->used) >= TOOM_MUL_CUTOFF) {
		res = toom_cook_multiply(a, b, c);
	} else if (MIN(a->used, b->used) >= KARATSUBA_MUL_CUTOFF) {
		/* use Karatsuba? */
		res = karatsuba_multiply(a, b, c);
	} else {
		/* can we use the fast multiplier? */
		int     digs = a->used + b->used + 1;

		if (can_use_fast_column_array(digs, MIN(a->used, b->used))) {
			res = fast_col_array_multiply(a, b, c, digs);
		} else  {
			res = basic_multiply_partial_lower(a, b, c, (a)->used + (b)->used + 1);
		}
	}
	c->sign = (c->used > 0) ? neg : MP_ZPOS;
	return res;
}

/* this is a modified version of fast_s_mul_digs that only produces
 * output digits *above* digs.  See the comments for fast_s_mul_digs
 * to see how it works.
 *
 * This is used in the Barrett reduction since for one of the multiplications
 * only the higher digits were needed.  This essentially halves the work.
 *
 * Based on Algorithm 14.12 on pp.595 of HAC.
 */
static int
fast_basic_multiply_partial_upper(mp_int * a, mp_int * b, mp_int * c, int digs)
{
	int     olduse, res, pa, ix, iz;
	mp_digit W[MP_WARRAY];
	mp_word  _W;

	/* grow the destination as required */
	pa = a->used + b->used;
	if (c->alloc < pa) {
		if ((res = mp_grow(c, pa)) != MP_OKAY) {
			return res;
		}
	}

	/* number of output digits to produce */
	pa = a->used + b->used;
	_W = 0;
	for (ix = digs; ix < pa; ix++) { 
		int      tx, ty, iy;
		mp_digit *tmpx, *tmpy;

		/* get offsets into the two bignums */
		ty = MIN(b->used-1, ix);
		tx = ix - ty;

		/* setup temp aliases */
		tmpx = a->dp + tx;
		tmpy = b->dp + ty;

		/* this is the number of times the loop will iterrate, essentially its 
		 while (tx++ < a->used && ty-- >= 0) { ... }
		*/
		iy = MIN(a->used-tx, ty+1);

		/* execute loop */
		for (iz = 0; iz < iy; iz++) {
			 _W += ((mp_word)*tmpx++)*((mp_word)*tmpy--);
		}

		/* store term */
		W[ix] = ((mp_digit)_W) & MP_MASK;

		/* make next carry */
		_W = _W >> ((mp_word)DIGIT_BIT);
	}

	/* setup dest */
	olduse  = c->used;
	c->used = pa;

	{
		mp_digit *tmpc;

		tmpc = c->dp + digs;
		for (ix = digs; ix < pa; ix++) {
			/* now extract the previous digit [below the carry] */
			*tmpc++ = W[ix];
		}

		/* clear unused digits [that existed in the old copy of c] */
		for (; ix < olduse; ix++) {
			*tmpc++ = 0;
		}
	}
	trim_unused_digits(c);
	return MP_OKAY;
}

/* Source: /usr/cvsroot/libtommath/dist/libtommath/bn_fast_s_mp_mul_high_digs.c,v $ */
/* Revision: 1.1.1.1 $ */
/* Date: 2011/03/12 22:58:18 $ */

/* multiplies |a| * |b| and does not compute the lower digs digits
 * [meant to get the higher part of the product]
 */
static int
basic_multiply_partial_upper(mp_int * a, mp_int * b, mp_int * c, int digs)
{
	mp_int  t;
	int     res, pa, pb, ix, iy;
	mp_digit carry;
	mp_word r;
	mp_digit tmpx, *tmpt, *tmpy;

	/* can we use the fast multiplier? */
	if (can_use_fast_column_array(a->used + b->used + 1, MIN(a->used, b->used))) {
		return fast_basic_multiply_partial_upper(a, b, c, digs);
	}

	if ((res = mp_init_size(&t, a->used + b->used + 1)) != MP_OKAY) {
		return res;
	}
	t.used = a->used + b->used + 1;

	pa = a->used;
	pb = b->used;
	for (ix = 0; ix < pa; ix++) {
		/* clear the carry */
		carry = 0;

		/* left hand side of A[ix] * B[iy] */
		tmpx = a->dp[ix];

		/* alias to the address of where the digits will be stored */
		tmpt = &(t.dp[digs]);

		/* alias for where to read the right hand side from */
		tmpy = b->dp + (digs - ix);

		for (iy = digs - ix; iy < pb; iy++) {
			/* calculate the double precision result */
			r = ((mp_word)*tmpt) +
				((mp_word)tmpx) * ((mp_word)*tmpy++) +
				((mp_word) carry);

			/* get the lower part */
			*tmpt++ = (mp_digit) (r & ((mp_word) MP_MASK));

			/* carry the carry */
			carry = (mp_digit) (r >> ((mp_word) DIGIT_BIT));
		}
		*tmpt = carry;
	}
	trim_unused_digits(&t);
	mp_exch(&t, c);
	mp_clear(&t);
	return MP_OKAY;
}

/* Source: /usr/cvsroot/libtommath/dist/libtommath/bn_s_mp_mul_high_digs.c,v $ */
/* Revision: 1.3 $ */
/* Date: 2011/03/18 16:43:04 $ */

/* reduces x mod m, assumes 0 < x < m**2, mu is 
 * precomputed via mp_reduce_setup.
 * From HAC pp.604 Algorithm 14.42
 */
static int
mp_reduce(mp_int * x, mp_int * m, mp_int * mu)
{
	mp_int  q;
	int     res, um = m->used;

	/* q = x */
	if ((res = mp_init_copy(&q, x)) != MP_OKAY) {
		return res;
	}

	/* q1 = x / b**(k-1)  */
	rshift_digits(&q, um - 1);         

	/* according to HAC this optimization is ok */
	if (((unsigned long) um) > (((mp_digit)1) << (DIGIT_BIT - 1))) {
		if ((res = signed_multiply(&q, mu, &q)) != MP_OKAY) {
			goto CLEANUP;
		}
	} else {
		if ((res = basic_multiply_partial_upper(&q, mu, &q, um)) != MP_OKAY) {
			goto CLEANUP;
		}
	}

	/* q3 = q2 / b**(k+1) */
	rshift_digits(&q, um + 1);         

	/* x = x mod b**(k+1), quick (no division) */
	if ((res = modulo_2_to_power(x, DIGIT_BIT * (um + 1), x)) != MP_OKAY) {
		goto CLEANUP;
	}

	/* q = q * m mod b**(k+1), quick (no division) */
	if ((res = basic_multiply_partial_lower(&q, m, &q, um + 1)) != MP_OKAY) {
		goto CLEANUP;
	}

	/* x = x - q */
	if ((res = signed_subtract(x, &q, x)) != MP_OKAY) {
		goto CLEANUP;
	}

	/* If x < 0, add b**(k+1) to it */
	if (compare_digit(x, 0) == MP_LT) {
		set_word(&q, 1);
		if ((res = lshift_digits(&q, um + 1)) != MP_OKAY) {
			goto CLEANUP;
		}
		if ((res = signed_add(x, &q, x)) != MP_OKAY) {
			goto CLEANUP;
		}
	}

	/* Back off if it's too big */
	while (signed_compare(x, m) != MP_LT) {
		if ((res = basic_subtract(x, m, x)) != MP_OKAY) {
			goto CLEANUP;
		}
	}

CLEANUP:
	mp_clear(&q);

	return res;
}

/* determines the setup value */
static int
mp_reduce_2k_setup_l(mp_int *a, mp_int *d)
{
	int    res;
	mp_int tmp;

	if ((res = mp_init(&tmp)) != MP_OKAY) {
		return res;
	}

	if ((res = mp_2expt(&tmp, mp_count_bits(a))) != MP_OKAY) {
		goto ERR;
	}

	if ((res = basic_subtract(&tmp, a, d)) != MP_OKAY) {
		goto ERR;
	}

ERR:
	mp_clear(&tmp);
	return res;
}

/* Source: /usr/cvsroot/libtommath/dist/libtommath/bn_mp_reduce_2k_setup_l.c,v $ */
/* Revision: 1.1.1.1 $ */
/* Date: 2011/03/12 22:58:18 $ */

/* reduces a modulo n where n is of the form 2**p - d 
   This differs from reduce_2k since "d" can be larger
   than a single digit.
*/
static int
mp_reduce_2k_l(mp_int *a, mp_int *n, mp_int *d)
{
	mp_int q;
	int    p, res;

	if ((res = mp_init(&q)) != MP_OKAY) {
		return res;
	}

	p = mp_count_bits(n);    
top:
	/* q = a/2**p, a = a mod 2**p */
	if ((res = rshift_bits(a, p, &q, a)) != MP_OKAY) {
		goto ERR;
	}

	/* q = q * d */
	if ((res = signed_multiply(&q, d, &q)) != MP_OKAY) { 
		goto ERR;
	}

	/* a = a + q */
	if ((res = basic_add(a, &q, a)) != MP_OKAY) {
		goto ERR;
	}

	if (compare_magnitude(a, n) != MP_LT) {
		basic_subtract(a, n, a);
		goto top;
	}

ERR:
	mp_clear(&q);
	return res;
}

/* Source: /usr/cvsroot/libtommath/dist/libtommath/bn_mp_reduce_2k_l.c,v $ */
/* Revision: 1.1.1.1 $ */
/* Date: 2011/03/12 22:58:18 $ */

/* squaring using Toom-Cook 3-way algorithm */
static int
toom_cook_square(mp_int *a, mp_int *b)
{
	mp_int w0, w1, w2, w3, w4, tmp1, a0, a1, a2;
	int res, B;

	/* init temps */
	if ((res = mp_init_multi(&w0, &w1, &w2, &w3, &w4, &a0, &a1, &a2, &tmp1, NULL)) != MP_OKAY) {
		return res;
	}

	/* B */
	B = a->used / 3;

	/* a = a2 * B**2 + a1 * B + a0 */
	if ((res = modulo_2_to_power(a, DIGIT_BIT * B, &a0)) != MP_OKAY) {
		goto ERR;
	}

	if ((res = mp_copy(a, &a1)) != MP_OKAY) {
		goto ERR;
	}
	rshift_digits(&a1, B);
	modulo_2_to_power(&a1, DIGIT_BIT * B, &a1);

	if ((res = mp_copy(a, &a2)) != MP_OKAY) {
		goto ERR;
	}
	rshift_digits(&a2, B*2);

	/* w0 = a0*a0 */
	if ((res = square(&a0, &w0)) != MP_OKAY) {
		goto ERR;
	}

	/* w4 = a2 * a2 */
	if ((res = square(&a2, &w4)) != MP_OKAY) {
		goto ERR;
	}

	/* w1 = (a2 + 2(a1 + 2a0))**2 */
	if ((res = doubled(&a0, &tmp1)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = signed_add(&tmp1, &a1, &tmp1)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = doubled(&tmp1, &tmp1)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = signed_add(&tmp1, &a2, &tmp1)) != MP_OKAY) {
		goto ERR;
	}

	if ((res = square(&tmp1, &w1)) != MP_OKAY) {
		goto ERR;
	}

	/* w3 = (a0 + 2(a1 + 2a2))**2 */
	if ((res = doubled(&a2, &tmp1)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = signed_add(&tmp1, &a1, &tmp1)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = doubled(&tmp1, &tmp1)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = signed_add(&tmp1, &a0, &tmp1)) != MP_OKAY) {
		goto ERR;
	}

	if ((res = square(&tmp1, &w3)) != MP_OKAY) {
		goto ERR;
	}


	/* w2 = (a2 + a1 + a0)**2 */
	if ((res = signed_add(&a2, &a1, &tmp1)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = signed_add(&tmp1, &a0, &tmp1)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = square(&tmp1, &w2)) != MP_OKAY) {
		goto ERR;
	}

	/* now solve the matrix

	0  0  0  0  1
	1  2  4  8  16
	1  1  1  1  1
	16 8  4  2  1
	1  0  0  0  0

	using 12 subtractions, 4 shifts, 2 small divisions and 1 small multiplication.
	*/

	/* r1 - r4 */
	if ((res = signed_subtract(&w1, &w4, &w1)) != MP_OKAY) {
		goto ERR;
	}
	/* r3 - r0 */
	if ((res = signed_subtract(&w3, &w0, &w3)) != MP_OKAY) {
		goto ERR;
	}
	/* r1/2 */
	if ((res = half(&w1, &w1)) != MP_OKAY) {
		goto ERR;
	}
	/* r3/2 */
	if ((res = half(&w3, &w3)) != MP_OKAY) {
		goto ERR;
	}
	/* r2 - r0 - r4 */
	if ((res = signed_subtract(&w2, &w0, &w2)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = signed_subtract(&w2, &w4, &w2)) != MP_OKAY) {
		goto ERR;
	}
	/* r1 - r2 */
	if ((res = signed_subtract(&w1, &w2, &w1)) != MP_OKAY) {
		goto ERR;
	}
	/* r3 - r2 */
	if ((res = signed_subtract(&w3, &w2, &w3)) != MP_OKAY) {
		goto ERR;
	}
	/* r1 - 8r0 */
	if ((res = lshift_bits(&w0, 3, &tmp1)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = signed_subtract(&w1, &tmp1, &w1)) != MP_OKAY) {
		goto ERR;
	}
	/* r3 - 8r4 */
	if ((res = lshift_bits(&w4, 3, &tmp1)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = signed_subtract(&w3, &tmp1, &w3)) != MP_OKAY) {
		goto ERR;
	}
	/* 3r2 - r1 - r3 */
	if ((res = multiply_digit(&w2, 3, &w2)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = signed_subtract(&w2, &w1, &w2)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = signed_subtract(&w2, &w3, &w2)) != MP_OKAY) {
		goto ERR;
	}
	/* r1 - r2 */
	if ((res = signed_subtract(&w1, &w2, &w1)) != MP_OKAY) {
		goto ERR;
	}
	/* r3 - r2 */
	if ((res = signed_subtract(&w3, &w2, &w3)) != MP_OKAY) {
		goto ERR;
	}
	/* r1/3 */
	if ((res = third(&w1, &w1, NULL)) != MP_OKAY) {
		goto ERR;
	}
	/* r3/3 */
	if ((res = third(&w3, &w3, NULL)) != MP_OKAY) {
		goto ERR;
	}

	/* at this point shift W[n] by B*n */
	if ((res = lshift_digits(&w1, 1*B)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = lshift_digits(&w2, 2*B)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = lshift_digits(&w3, 3*B)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = lshift_digits(&w4, 4*B)) != MP_OKAY) {
		goto ERR;
	}

	if ((res = signed_add(&w0, &w1, b)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = signed_add(&w2, &w3, &tmp1)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = signed_add(&w4, &tmp1, &tmp1)) != MP_OKAY) {
		goto ERR;
	}
	if ((res = signed_add(&tmp1, b, b)) != MP_OKAY) {
		goto ERR;
	}

ERR:
	mp_clear_multi(&w0, &w1, &w2, &w3, &w4, &a0, &a1, &a2, &tmp1, NULL);
	return res;
}


/* Source: /usr/cvsroot/libtommath/dist/libtommath/bn_mp_toom_sqr.c,v $ */
/* Revision: 1.1.1.1 $ */
/* Date: 2011/03/12 22:58:18 $ */

/* Karatsuba squaring, computes b = a*a using three 
 * half size squarings
 *
 * See comments of karatsuba_mul for details.  It 
 * is essentially the same algorithm but merely 
 * tuned to perform recursive squarings.
 */
static int
karatsuba_square(mp_int * a, mp_int * b)
{
	mp_int  x0, x1, t1, t2, x0x0, x1x1;
	int     B, err;

	err = MP_MEM;

	/* min # of digits */
	B = a->used;

	/* now divide in two */
	B = (unsigned)B >> 1;

	/* init copy all the temps */
	if (mp_init_size(&x0, B) != MP_OKAY) {
		goto ERR;
	}
	if (mp_init_size(&x1, a->used - B) != MP_OKAY) {
		goto X0;
	}
	/* init temps */
	if (mp_init_size(&t1, a->used * 2) != MP_OKAY) {
		goto X1;
	}
	if (mp_init_size(&t2, a->used * 2) != MP_OKAY) {
		goto T1;
	}
	if (mp_init_size(&x0x0, B * 2) != MP_OKAY) {
		goto T2;
	}
	if (mp_init_size(&x1x1, (a->used - B) * 2) != MP_OKAY) {
		goto X0X0;
	}

	memcpy(x0.dp, a->dp, B * sizeof(*x0.dp));
	memcpy(x1.dp, &a->dp[B], (a->used - B) * sizeof(*x1.dp));

	x0.used = B;
	x1.used = a->used - B;

	trim_unused_digits(&x0);

	/* now calc the products x0*x0 and x1*x1 */
	if (square(&x0, &x0x0) != MP_OKAY) {
		goto X1X1;           /* x0x0 = x0*x0 */
	}
	if (square(&x1, &x1x1) != MP_OKAY) {
		goto X1X1;           /* x1x1 = x1*x1 */
	}
	/* now calc (x1+x0)**2 */
	if (basic_add(&x1, &x0, &t1) != MP_OKAY) {
		goto X1X1;           /* t1 = x1 - x0 */
	}
	if (square(&t1, &t1) != MP_OKAY) {
		goto X1X1;           /* t1 = (x1 - x0) * (x1 - x0) */
	}
	/* add x0y0 */
	if (basic_add(&x0x0, &x1x1, &t2) != MP_OKAY) {
		goto X1X1;           /* t2 = x0x0 + x1x1 */
	}
	if (basic_subtract(&t1, &t2, &t1) != MP_OKAY) {
		goto X1X1;           /* t1 = (x1+x0)**2 - (x0x0 + x1x1) */
	}
	/* shift by B */
	if (lshift_digits(&t1, B) != MP_OKAY) {
		goto X1X1;           /* t1 = (x0x0 + x1x1 - (x1-x0)*(x1-x0))<<B */
	}
	if (lshift_digits(&x1x1, B * 2) != MP_OKAY) {
		goto X1X1;           /* x1x1 = x1x1 << 2*B */
	}
	if (signed_add(&x0x0, &t1, &t1) != MP_OKAY) {
		goto X1X1;           /* t1 = x0x0 + t1 */
	}
	if (signed_add(&t1, &x1x1, b) != MP_OKAY) {
		goto X1X1;           /* t1 = x0x0 + t1 + x1x1 */
	}
	err = MP_OKAY;

X1X1:
	mp_clear(&x1x1);
X0X0:
	mp_clear(&x0x0);
T2:
	mp_clear(&t2);
T1:
	mp_clear(&t1);
X1:
	mp_clear(&x1);
X0:
	mp_clear(&x0);
ERR:
	return err;
}

/* Source: /usr/cvsroot/libtommath/dist/libtommath/bn_mp_karatsuba_sqr.c,v $ */
/* Revision: 1.2 $ */
/* Date: 2011/03/12 23:43:54 $ */

/* the jist of squaring...
 * you do like mult except the offset of the tmpx [one that 
 * starts closer to zero] can't equal the offset of tmpy.  
 * So basically you set up iy like before then you min it with
 * (ty-tx) so that it never happens.  You double all those 
 * you add in the inner loop

After that loop you do the squares and add them in.
*/

static int
fast_basic_square(mp_int * a, mp_int * b)
{
	int       olduse, res, pa, ix, iz;
	mp_digit   W[MP_WARRAY], *tmpx;
	mp_word   W1;

	/* grow the destination as required */
	pa = a->used + a->used;
	if (b->alloc < pa) {
		if ((res = mp_grow(b, pa)) != MP_OKAY) {
			return res;
		}
	}

	/* number of output digits to produce */
	W1 = 0;
	for (ix = 0; ix < pa; ix++) { 
		int      tx, ty, iy;
		mp_word  _W;
		mp_digit *tmpy;

		/* clear counter */
		_W = 0;

		/* get offsets into the two bignums */
		ty = MIN(a->used-1, ix);
		tx = ix - ty;

		/* setup temp aliases */
		tmpx = a->dp + tx;
		tmpy = a->dp + ty;

		/* this is the number of times the loop will iterrate, essentially
		 while (tx++ < a->used && ty-- >= 0) { ... }
		*/
		iy = MIN(a->used-tx, ty+1);

		/* now for squaring tx can never equal ty 
		* we halve the distance since they approach at a rate of 2x
		* and we have to round because odd cases need to be executed
		*/
		iy = MIN(iy, (int)((unsigned)(ty-tx+1)>>1));

		/* execute loop */
		for (iz = 0; iz < iy; iz++) {
			 _W += ((mp_word)*tmpx++)*((mp_word)*tmpy--);
		}

		/* double the inner product and add carry */
		_W = _W + _W + W1;

		/* even columns have the square term in them */
		if ((ix&1) == 0) {
			 _W += ((mp_word)a->dp[(unsigned)ix>>1])*((mp_word)a->dp[(unsigned)ix>>1]);
		}

		/* store it */
		W[ix] = (mp_digit)(_W & MP_MASK);

		/* make next carry */
		W1 = _W >> ((mp_word)DIGIT_BIT);
	}

	/* setup dest */
	olduse  = b->used;
	b->used = a->used+a->used;

	{
		mp_digit *tmpb;
		tmpb = b->dp;
		for (ix = 0; ix < pa; ix++) {
			*tmpb++ = W[ix] & MP_MASK;
		}

		/* clear unused digits [that existed in the old copy of c] */
		for (; ix < olduse; ix++) {
			*tmpb++ = 0;
		}
	}
	trim_unused_digits(b);
	return MP_OKAY;
}

/* Source: /usr/cvsroot/libtommath/dist/libtommath/bn_fast_s_mp_sqr.c,v $ */
/* Revision: 1.3 $ */
/* Date: 2011/03/18 16:43:04 $ */

/* low level squaring, b = a*a, HAC pp.596-597, Algorithm 14.16 */
static int
basic_square(mp_int * a, mp_int * b)
{
	mp_int  t;
	int     res, ix, iy, pa;
	mp_word r;
	mp_digit carry, tmpx, *tmpt;

	pa = a->used;
	if ((res = mp_init_size(&t, 2*pa + 1)) != MP_OKAY) {
		return res;
	}

	/* default used is maximum possible size */
	t.used = 2*pa + 1;

	for (ix = 0; ix < pa; ix++) {
		/* first calculate the digit at 2*ix */
		/* calculate double precision result */
		r = ((mp_word) t.dp[2*ix]) +
		((mp_word)a->dp[ix])*((mp_word)a->dp[ix]);

		/* store lower part in result */
		t.dp[ix+ix] = (mp_digit) (r & ((mp_word) MP_MASK));

		/* get the carry */
		carry = (mp_digit)(r >> ((mp_word) DIGIT_BIT));

		/* left hand side of A[ix] * A[iy] */
		tmpx = a->dp[ix];

		/* alias for where to store the results */
		tmpt = t.dp + (2*ix + 1);

		for (iy = ix + 1; iy < pa; iy++) {
			/* first calculate the product */
			r = ((mp_word)tmpx) * ((mp_word)a->dp[iy]);

			/* now calculate the double precision result, note we use
			* addition instead of *2 since it's easier to optimize
			*/
			r = ((mp_word) *tmpt) + r + r + ((mp_word) carry);

			/* store lower part */
			*tmpt++ = (mp_digit) (r & ((mp_word) MP_MASK));

			/* get carry */
			carry = (mp_digit)(r >> ((mp_word) DIGIT_BIT));
		}
		/* propagate upwards */
		while (carry != ((mp_digit) 0)) {
			r = ((mp_word) *tmpt) + ((mp_word) carry);
			*tmpt++ = (mp_digit) (r & ((mp_word) MP_MASK));
			carry = (mp_digit)(r >> ((mp_word) DIGIT_BIT));
		}
	}

	trim_unused_digits(&t);
	mp_exch(&t, b);
	mp_clear(&t);
	return MP_OKAY;
}

/* Source: /usr/cvsroot/libtommath/dist/libtommath/bn_s_mp_sqr.c,v $ */
/* Revision: 1.1.1.1 $ */
/* Date: 2011/03/12 22:58:18 $ */

#define TOOM_SQR_CUTOFF      400
#define KARATSUBA_SQR_CUTOFF 120

/* computes b = a*a */
static int
square(mp_int * a, mp_int * b)
{
	int     res;

	/* use Toom-Cook? */
	if (a->used >= TOOM_SQR_CUTOFF) {
		res = toom_cook_square(a, b);
		/* Karatsuba? */
	} else if (a->used >= KARATSUBA_SQR_CUTOFF) {
		res = karatsuba_square(a, b);
	} else {
		/* can we use the fast comba multiplier? */
		if (can_use_fast_column_array(a->used + a->used + 1, a->used)) {
			res = fast_basic_square(a, b);
		} else {
			res = basic_square(a, b);
		}
	}
	b->sign = MP_ZPOS;
	return res;
}

/* find window size */
static inline int
find_window_size(mp_int *X)
{
	int	x;

	x = mp_count_bits(X);
	return (x <= 7) ? 2 : (x <= 36) ? 3 : (x <= 140) ? 4 : (x <= 450) ? 5 : (x <= 1303) ? 6 : (x <= 3529) ? 7 : 8;
}

/* Source: /usr/cvsroot/libtommath/dist/libtommath/bn_mp_sqr.c,v $ */
/* Revision: 1.3 $ */
/* Date: 2011/03/18 16:43:04 $ */

#define TAB_SIZE 256

static int
basic_exponent_mod(mp_int * G, mp_int * X, mp_int * P, mp_int * Y, int redmode)
{
	mp_digit buf;
	mp_int  M[TAB_SIZE], res, mu;
	int     err, bitbuf, bitcpy, bitcnt, mode, digidx, x, y, winsize;
	int	(*redux)(mp_int*,mp_int*,mp_int*);

	winsize = find_window_size(X);

	/* init M array */
	/* init first cell */
	if ((err = mp_init(&M[1])) != MP_OKAY) {
		return err; 
	}

	/* now init the second half of the array */
	for (x = 1<<(winsize-1); x < (1 << winsize); x++) {
		if ((err = mp_init(&M[x])) != MP_OKAY) {
			for (y = 1<<(winsize-1); y < x; y++) {
				mp_clear(&M[y]);
			}
			mp_clear(&M[1]);
			return err;
		}
	}

	/* create mu, used for Barrett reduction */
	if ((err = mp_init(&mu)) != MP_OKAY) {
		goto LBL_M;
	}

	if (redmode == 0) {
		if ((err = mp_reduce_setup(&mu, P)) != MP_OKAY) {
			goto LBL_MU;
		}
		redux = mp_reduce;
	} else {
		if ((err = mp_reduce_2k_setup_l(P, &mu)) != MP_OKAY) {
			goto LBL_MU;
		}
		redux = mp_reduce_2k_l;
	}    

	/* create M table
	*
	* The M table contains powers of the base, 
	* e.g. M[x] = G**x mod P
	*
	* The first half of the table is not 
	* computed though accept for M[0] and M[1]
	*/
	if ((err = modulo(G, P, &M[1])) != MP_OKAY) {
		goto LBL_MU;
	}

	/* compute the value at M[1<<(winsize-1)] by squaring 
	* M[1] (winsize-1) times 
	*/
	if ((err = mp_copy( &M[1], &M[1 << (winsize - 1)])) != MP_OKAY) {
		goto LBL_MU;
	}

	for (x = 0; x < (winsize - 1); x++) {
		/* square it */
		if ((err = square(&M[1 << (winsize - 1)], 
		       &M[1 << (winsize - 1)])) != MP_OKAY) {
			goto LBL_MU;
		}

		/* reduce modulo P */
		if ((err = redux(&M[1 << (winsize - 1)], P, &mu)) != MP_OKAY) {
			goto LBL_MU;
		}
	}

	/* create upper table, that is M[x] = M[x-1] * M[1] (mod P)
	* for x = (2**(winsize - 1) + 1) to (2**winsize - 1)
	*/
	for (x = (1 << (winsize - 1)) + 1; x < (1 << winsize); x++) {
		if ((err = signed_multiply(&M[x - 1], &M[1], &M[x])) != MP_OKAY) {
			goto LBL_MU;
		}
		if ((err = redux(&M[x], P, &mu)) != MP_OKAY) {
			goto LBL_MU;
		}
	}

	/* setup result */
	if ((err = mp_init(&res)) != MP_OKAY) {
		goto LBL_MU;
	}
	set_word(&res, 1);

	/* set initial mode and bit cnt */
	mode = 0;
	bitcnt = 1;
	buf = 0;
	digidx = X->used - 1;
	bitcpy = 0;
	bitbuf = 0;

	for (;;) {
		/* grab next digit as required */
		if (--bitcnt == 0) {
			/* if digidx == -1 we are out of digits */
			if (digidx == -1) {
				break;
			}
			/* read next digit and reset the bitcnt */
			buf = X->dp[digidx--];
			bitcnt = (int) DIGIT_BIT;
		}

		/* grab the next msb from the exponent */
		y = (unsigned)(buf >> (mp_digit)(DIGIT_BIT - 1)) & 1;
		buf <<= (mp_digit)1;

		/* if the bit is zero and mode == 0 then we ignore it
		* These represent the leading zero bits before the first 1 bit
		* in the exponent.  Technically this opt is not required but it
		* does lower the # of trivial squaring/reductions used
		*/
		if (mode == 0 && y == 0) {
			continue;
		}

		/* if the bit is zero and mode == 1 then we square */
		if (mode == 1 && y == 0) {
			if ((err = square(&res, &res)) != MP_OKAY) {
				goto LBL_RES;
			}
			if ((err = redux(&res, P, &mu)) != MP_OKAY) {
				goto LBL_RES;
			}
			continue;
		}

		/* else we add it to the window */
		bitbuf |= (y << (winsize - ++bitcpy));
		mode = 2;

		if (bitcpy == winsize) {
			/* ok window is filled so square as required and multiply  */
			/* square first */
			for (x = 0; x < winsize; x++) {
				if ((err = square(&res, &res)) != MP_OKAY) {
					goto LBL_RES;
				}
				if ((err = redux(&res, P, &mu)) != MP_OKAY) {
					goto LBL_RES;
				}
			}

			/* then multiply */
			if ((err = signed_multiply(&res, &M[bitbuf], &res)) != MP_OKAY) {
				goto LBL_RES;
			}
			if ((err = redux(&res, P, &mu)) != MP_OKAY) {
				goto LBL_RES;
			}

			/* empty window and reset */
			bitcpy = 0;
			bitbuf = 0;
			mode = 1;
		}
	}

	/* if bits remain then square/multiply */
	if (mode == 2 && bitcpy > 0) {
		/* square then multiply if the bit is set */
		for (x = 0; x < bitcpy; x++) {
			if ((err = square(&res, &res)) != MP_OKAY) {
				goto LBL_RES;
			}
			if ((err = redux(&res, P, &mu)) != MP_OKAY) {
				goto LBL_RES;
			}

			bitbuf <<= 1;
			if ((bitbuf & (1 << winsize)) != 0) {
				/* then multiply */
				if ((err = signed_multiply(&res, &M[1], &res)) != MP_OKAY) {
					goto LBL_RES;
				}
				if ((err = redux(&res, P, &mu)) != MP_OKAY) {
					goto LBL_RES;
				}
			}
		}
	}

	mp_exch(&res, Y);
	err = MP_OKAY;
LBL_RES:
	mp_clear(&res);
LBL_MU:
	mp_clear(&mu);
LBL_M:
	mp_clear(&M[1]);
	for (x = 1<<(winsize-1); x < (1 << winsize); x++) {
		mp_clear(&M[x]);
	}
	return err;
}

/* determines if a number is a valid DR modulus */
static int
is_diminished_radix_modulus(mp_int *a)
{
	int ix;

	/* must be at least two digits */
	if (a->used < 2) {
		return 0;
	}

	/* must be of the form b**k - a [a <= b] so all
	* but the first digit must be equal to -1 (mod b).
	*/
	for (ix = 1; ix < a->used; ix++) {
		if (a->dp[ix] != MP_MASK) {
			  return 0;
		}
	}
	return 1;
}

/* Source: /usr/cvsroot/libtommath/dist/libtommath/bn_mp_dr_is_modulus.c,v $ */
/* Revision: 1.1.1.1 $ */
/* Date: 2011/03/12 22:58:18 $ */

/* determines if mp_reduce_2k can be used */
static int
mp_reduce_is_2k(mp_int *a)
{
	int ix, iy, iw;
	mp_digit iz;

	if (a->used == 0) {
		return MP_NO;
	}
	if (a->used == 1) {
		return MP_YES;
	}
	if (a->used > 1) {
		iy = mp_count_bits(a);
		iz = 1;
		iw = 1;

		/* Test every bit from the second digit up, must be 1 */
		for (ix = DIGIT_BIT; ix < iy; ix++) {
			if ((a->dp[iw] & iz) == 0) {
				return MP_NO;
			}
			iz <<= 1;
			if (iz > (mp_digit)MP_MASK) {
				++iw;
				iz = 1;
			}
		}
	}
	return MP_YES;
}

/* Source: /usr/cvsroot/libtommath/dist/libtommath/bn_mp_reduce_is_2k.c,v $ */
/* Revision: 1.1.1.1 $ */
/* Date: 2011/03/12 22:58:18 $ */


/* d = a * b (mod c) */
static int
multiply_modulo(mp_int *d, mp_int * a, mp_int * b, mp_int * c)
{
	mp_int  t;
	int     res;

	if ((res = mp_init(&t)) != MP_OKAY) {
		return res;
	}

	if ((res = signed_multiply(a, b, &t)) != MP_OKAY) {
		mp_clear(&t);
		return res;
	}
	res = modulo(&t, c, d);
	mp_clear(&t);
	return res;
}

/* Source: /usr/cvsroot/libtommath/dist/libtommath/bn_mp_mulmod.c,v $ */
/* Revision: 1.1.1.1 $ */
/* Date: 2011/03/12 22:58:18 $ */

/* setups the montgomery reduction stuff */
static int
mp_montgomery_setup(mp_int * n, mp_digit * rho)
{
	mp_digit x, b;

	/* fast inversion mod 2**k
	*
	* Based on the fact that
	*
	* XA = 1 (mod 2**n)  =>  (X(2-XA)) A = 1 (mod 2**2n)
	*                    =>  2*X*A - X*X*A*A = 1
	*                    =>  2*(1) - (1)     = 1
	*/
	b = n->dp[0];

	if ((b & 1) == 0) {
		return MP_VAL;
	}

	x = (((b + 2) & 4) << 1) + b; /* here x*a==1 mod 2**4 */
	x *= 2 - b * x;               /* here x*a==1 mod 2**8 */
	x *= 2 - b * x;               /* here x*a==1 mod 2**16 */
	x *= 2 - b * x;               /* here x*a==1 mod 2**32 */
	if (/*CONSTCOND*/sizeof(mp_digit) == 8) {
		x *= 2 - b * x;	/* here x*a==1 mod 2**64 */
	}

	/* rho = -1/m mod b */
	*rho = (unsigned long)(((mp_word)1 << ((mp_word) DIGIT_BIT)) - x) & MP_MASK;

	return MP_OKAY;
}

/* Source: /usr/cvsroot/libtommath/dist/libtommath/bn_mp_montgomery_setup.c,v $ */
/* Revision: 1.1.1.1 $ */
/* Date: 2011/03/12 22:58:18 $ */

/* computes xR**-1 == x (mod N) via Montgomery Reduction
 *
 * This is an optimized implementation of montgomery_reduce
 * which uses the comba method to quickly calculate the columns of the
 * reduction.
 *
 * Based on Algorithm 14.32 on pp.601 of HAC.
*/
static int
fast_mp_montgomery_reduce(mp_int * x, mp_int * n, mp_digit rho)
{
	int     ix, res, olduse;
	/*LINTED*/
	mp_word W[MP_WARRAY];

	/* get old used count */
	olduse = x->used;

	/* grow a as required */
	if (x->alloc < n->used + 1) {
		if ((res = mp_grow(x, n->used + 1)) != MP_OKAY) {
			return res;
		}
	}

	/* first we have to get the digits of the input into
	* an array of double precision words W[...]
	*/
	{
		mp_word *_W;
		mp_digit *tmpx;

		/* alias for the W[] array */
		_W = W;

		/* alias for the digits of  x*/
		tmpx = x->dp;

		/* copy the digits of a into W[0..a->used-1] */
		for (ix = 0; ix < x->used; ix++) {
			*_W++ = *tmpx++;
		}

		/* zero the high words of W[a->used..m->used*2] */
		for (; ix < n->used * 2 + 1; ix++) {
			*_W++ = 0;
		}
	}

	/* now we proceed to zero successive digits
	* from the least significant upwards
	*/
	for (ix = 0; ix < n->used; ix++) {
		/* mu = ai * m' mod b
		*
		* We avoid a double precision multiplication (which isn't required)
		* by casting the value down to a mp_digit.  Note this requires
		* that W[ix-1] have  the carry cleared (see after the inner loop)
		*/
		mp_digit mu;
		mu = (mp_digit) (((W[ix] & MP_MASK) * rho) & MP_MASK);

		/* a = a + mu * m * b**i
		*
		* This is computed in place and on the fly.  The multiplication
		* by b**i is handled by offseting which columns the results
		* are added to.
		*
		* Note the comba method normally doesn't handle carries in the
		* inner loop In this case we fix the carry from the previous
		* column since the Montgomery reduction requires digits of the
		* result (so far) [see above] to work.  This is
		* handled by fixing up one carry after the inner loop.  The
		* carry fixups are done in order so after these loops the
		* first m->used words of W[] have the carries fixed
		*/
		{
			int iy;
			mp_digit *tmpn;
			mp_word *_W;

			/* alias for the digits of the modulus */
			tmpn = n->dp;

			/* Alias for the columns set by an offset of ix */
			_W = W + ix;

			/* inner loop */
			for (iy = 0; iy < n->used; iy++) {
				  *_W++ += ((mp_word)mu) * ((mp_word)*tmpn++);
			}
		}

		/* now fix carry for next digit, W[ix+1] */
		W[ix + 1] += W[ix] >> ((mp_word) DIGIT_BIT);
	}

	/* now we have to propagate the carries and
	* shift the words downward [all those least
	* significant digits we zeroed].
	*/
	{
		mp_digit *tmpx;
		mp_word *_W, *_W1;

		/* nox fix rest of carries */

		/* alias for current word */
		_W1 = W + ix;

		/* alias for next word, where the carry goes */
		_W = W + ++ix;

		for (; ix <= n->used * 2 + 1; ix++) {
			*_W++ += *_W1++ >> ((mp_word) DIGIT_BIT);
		}

		/* copy out, A = A/b**n
		*
		* The result is A/b**n but instead of converting from an
		* array of mp_word to mp_digit than calling rshift_digits
		* we just copy them in the right order
		*/

		/* alias for destination word */
		tmpx = x->dp;

		/* alias for shifted double precision result */
		_W = W + n->used;

		for (ix = 0; ix < n->used + 1; ix++) {
			*tmpx++ = (mp_digit)(*_W++ & ((mp_word) MP_MASK));
		}

		/* zero oldused digits, if the input a was larger than
		* m->used+1 we'll have to clear the digits
		*/
		for (; ix < olduse; ix++) {
			*tmpx++ = 0;
		}
	}

	/* set the max used and clamp */
	x->used = n->used + 1;
	trim_unused_digits(x);

	/* if A >= m then A = A - m */
	if (compare_magnitude(x, n) != MP_LT) {
		return basic_subtract(x, n, x);
	}
	return MP_OKAY;
}

/* Source: /usr/cvsroot/libtommath/dist/libtommath/bn_fast_mp_montgomery_reduce.c,v $ */
/* Revision: 1.2 $ */
/* Date: 2011/03/18 16:22:09 $ */

/* computes xR**-1 == x (mod N) via Montgomery Reduction */
static int
mp_montgomery_reduce(mp_int * x, mp_int * n, mp_digit rho)
{
	int     ix, res, digs;
	mp_digit mu;

	/* can the fast reduction [comba] method be used?
	*
	* Note that unlike in mul you're safely allowed *less*
	* than the available columns [255 per default] since carries
	* are fixed up in the inner loop.
	*/
	digs = n->used * 2 + 1;
	if (can_use_fast_column_array(digs, n->used)) {
		return fast_mp_montgomery_reduce(x, n, rho);
	}

	/* grow the input as required */
	if (x->alloc < digs) {
		if ((res = mp_grow(x, digs)) != MP_OKAY) {
			return res;
		}
	}
	x->used = digs;

	for (ix = 0; ix < n->used; ix++) {
		/* mu = ai * rho mod b
		*
		* The value of rho must be precalculated via
		* montgomery_setup() such that
		* it equals -1/n0 mod b this allows the
		* following inner loop to reduce the
		* input one digit at a time
		*/
		mu = (mp_digit) (((mp_word)x->dp[ix]) * ((mp_word)rho) & MP_MASK);

		/* a = a + mu * m * b**i */
		{
			int iy;
			mp_digit *tmpn, *tmpx, carry;
			mp_word r;

			/* alias for digits of the modulus */
			tmpn = n->dp;

			/* alias for the digits of x [the input] */
			tmpx = x->dp + ix;

			/* set the carry to zero */
			carry = 0;

			/* Multiply and add in place */
			for (iy = 0; iy < n->used; iy++) {
				/* compute product and sum */
				r = ((mp_word)mu) * ((mp_word)*tmpn++) +
					  ((mp_word) carry) + ((mp_word) * tmpx);

				/* get carry */
				carry = (mp_digit)(r >> ((mp_word) DIGIT_BIT));

				/* fix digit */
				*tmpx++ = (mp_digit)(r & ((mp_word) MP_MASK));
			}
			/* At this point the ix'th digit of x should be zero */


			/* propagate carries upwards as required*/
			while (carry) {
				*tmpx += carry;
				carry = *tmpx >> DIGIT_BIT;
				*tmpx++ &= MP_MASK;
			}
		}
	}

	/* at this point the n.used'th least
	* significant digits of x are all zero
	* which means we can shift x to the
	* right by n.used digits and the
	* residue is unchanged.
	*/

	/* x = x/b**n.used */
	trim_unused_digits(x);
	rshift_digits(x, n->used);

	/* if x >= n then x = x - n */
	if (compare_magnitude(x, n) != MP_LT) {
		return basic_subtract(x, n, x);
	}

	return MP_OKAY;
}

/* Source: /usr/cvsroot/libtommath/dist/libtommath/bn_mp_montgomery_reduce.c,v $ */
/* Revision: 1.3 $ */
/* Date: 2011/03/18 16:43:04 $ */

/* determines the setup value */
static void
diminished_radix_setup(mp_int *a, mp_digit *d)
{
	/* the casts are required if DIGIT_BIT is one less than
	* the number of bits in a mp_digit [e.g. DIGIT_BIT==31]
	*/
	*d = (mp_digit)((((mp_word)1) << ((mp_word)DIGIT_BIT)) - 
		((mp_word)a->dp[0]));
}

/* Source: /usr/cvsroot/libtommath/dist/libtommath/bn_mp_dr_setup.c,v $ */
/* Revision: 1.1.1.1 $ */
/* Date: 2011/03/12 22:58:18 $ */

/* reduce "x" in place modulo "n" using the Diminished Radix algorithm.
 *
 * Based on algorithm from the paper
 *
 * "Generating Efficient Primes for Discrete Log Cryptosystems"
 *                 Chae Hoon Lim, Pil Joong Lee,
 *          POSTECH Information Research Laboratories
 *
 * The modulus must be of a special format [see manual]
 *
 * Has been modified to use algorithm 7.10 from the LTM book instead
 *
 * Input x must be in the range 0 <= x <= (n-1)**2
 */
static int
diminished_radix_reduce(mp_int * x, mp_int * n, mp_digit k)
{
	int      err, i, m;
	mp_word  r;
	mp_digit mu, *tmpx1, *tmpx2;

	/* m = digits in modulus */
	m = n->used;

	/* ensure that "x" has at least 2m digits */
	if (x->alloc < m + m) {
		if ((err = mp_grow(x, m + m)) != MP_OKAY) {
			return err;
		}
	}

	/* top of loop, this is where the code resumes if
	* another reduction pass is required.
	*/
top:
	/* aliases for digits */
	/* alias for lower half of x */
	tmpx1 = x->dp;

	/* alias for upper half of x, or x/B**m */
	tmpx2 = x->dp + m;

	/* set carry to zero */
	mu = 0;

	/* compute (x mod B**m) + k * [x/B**m] inline and inplace */
	for (i = 0; i < m; i++) {
		r = ((mp_word)*tmpx2++) * ((mp_word)k) + *tmpx1 + mu;
		*tmpx1++  = (mp_digit)(r & MP_MASK);
		mu = (mp_digit)(r >> ((mp_word)DIGIT_BIT));
	}

	/* set final carry */
	*tmpx1++ = mu;

	/* zero words above m */
	for (i = m + 1; i < x->used; i++) {
		*tmpx1++ = 0;
	}

	/* clamp, sub and return */
	trim_unused_digits(x);

	/* if x >= n then subtract and reduce again
	* Each successive "recursion" makes the input smaller and smaller.
	*/
	if (compare_magnitude(x, n) != MP_LT) {
		basic_subtract(x, n, x);
		goto top;
	}
	return MP_OKAY;
}

/* Source: /usr/cvsroot/libtommath/dist/libtommath/bn_mp_dr_reduce.c,v $ */
/* Revision: 1.1.1.1 $ */
/* Date: 2011/03/12 22:58:18 $ */

/* determines the setup value */
static int
mp_reduce_2k_setup(mp_int *a, mp_digit *d)
{
	int res, p;
	mp_int tmp;

	if ((res = mp_init(&tmp)) != MP_OKAY) {
		return res;
	}

	p = mp_count_bits(a);
	if ((res = mp_2expt(&tmp, p)) != MP_OKAY) {
		mp_clear(&tmp);
		return res;
	}

	if ((res = basic_subtract(&tmp, a, &tmp)) != MP_OKAY) {
		mp_clear(&tmp);
		return res;
	}

	*d = tmp.dp[0];
	mp_clear(&tmp);
	return MP_OKAY;
}

/* Source: /usr/cvsroot/libtommath/dist/libtommath/bn_mp_reduce_2k_setup.c,v $ */
/* Revision: 1.1.1.1 $ */
/* Date: 2011/03/12 22:58:18 $ */

/* reduces a modulo n where n is of the form 2**p - d */
static int
mp_reduce_2k(mp_int *a, mp_int *n, mp_digit d)
{
	mp_int q;
	int    p, res;

	if ((res = mp_init(&q)) != MP_OKAY) {
		return res;
	}

	p = mp_count_bits(n);    
top:
	/* q = a/2**p, a = a mod 2**p */
	if ((res = rshift_bits(a, p, &q, a)) != MP_OKAY) {
		goto ERR;
	}

	if (d != 1) {
		/* q = q * d */
		if ((res = multiply_digit(&q, d, &q)) != MP_OKAY) { 
			 goto ERR;
		}
	}

	/* a = a + q */
	if ((res = basic_add(a, &q, a)) != MP_OKAY) {
		goto ERR;
	}

	if (compare_magnitude(a, n) != MP_LT) {
		basic_subtract(a, n, a);
		goto top;
	}

ERR:
	mp_clear(&q);
	return res;
}

/* Source: /usr/cvsroot/libtommath/dist/libtommath/bn_mp_reduce_2k.c,v $ */
/* Revision: 1.1.1.1 $ */
/* Date: 2011/03/12 22:58:18 $ */

/*
 * shifts with subtractions when the result is greater than b.
 *
 * The method is slightly modified to shift B unconditionally upto just under
 * the leading bit of b.  This saves alot of multiple precision shifting.
 */
static int
mp_montgomery_calc_normalization(mp_int * a, mp_int * b)
{
	int     x, bits, res;

	/* how many bits of last digit does b use */
	bits = mp_count_bits(b) % DIGIT_BIT;

	if (b->used > 1) {
		if ((res = mp_2expt(a, (b->used - 1) * DIGIT_BIT + bits - 1)) != MP_OKAY) {
			return res;
		}
	} else {
		set_word(a, 1);
		bits = 1;
	}


	/* now compute C = A * B mod b */
	for (x = bits - 1; x < (int)DIGIT_BIT; x++) {
		if ((res = doubled(a, a)) != MP_OKAY) {
			return res;
		}
		if (compare_magnitude(a, b) != MP_LT) {
			if ((res = basic_subtract(a, b, a)) != MP_OKAY) {
				return res;
			}
		}
	}

	return MP_OKAY;
}

/* Source: /usr/cvsroot/libtommath/dist/libtommath/bn_mp_montgomery_calc_normalization.c,v $ */
/* Revision: 1.1.1.1 $ */
/* Date: 2011/03/12 22:58:18 $ */

/* computes Y == G**X mod P, HAC pp.616, Algorithm 14.85
 *
 * Uses a left-to-right k-ary sliding window to compute the modular exponentiation.
 * The value of k changes based on the size of the exponent.
 *
 * Uses Montgomery or Diminished Radix reduction [whichever appropriate]
 */

#define TAB_SIZE 256

static int
fast_exponent_modulo(mp_int * G, mp_int * X, mp_int * P, mp_int * Y, int redmode)
{
	mp_int  M[TAB_SIZE], res;
	mp_digit buf, mp;
	int     err, bitbuf, bitcpy, bitcnt, mode, digidx, x, y, winsize;

	/* use a pointer to the reduction algorithm.  This allows us to use
	* one of many reduction algorithms without modding the guts of
	* the code with if statements everywhere.
	*/
	int     (*redux)(mp_int*,mp_int*,mp_digit);

#if defined(__minix)
	mp = 0; /* LSC: Fix -Os compilation: -Werror=maybe-uninitialized */
#endif /* defined(__minix) */
	winsize = find_window_size(X);

	/* init M array */
	/* init first cell */
	if ((err = mp_init(&M[1])) != MP_OKAY) {
		return err;
	}

	/* now init the second half of the array */
	for (x = 1<<(winsize-1); x < (1 << winsize); x++) {
		if ((err = mp_init(&M[x])) != MP_OKAY) {
			for (y = 1<<(winsize-1); y < x; y++) {
				mp_clear(&M[y]);
			}
			mp_clear(&M[1]);
			return err;
		}
	}

	/* determine and setup reduction code */
	if (redmode == 0) {
		/* now setup montgomery  */
		if ((err = mp_montgomery_setup(P, &mp)) != MP_OKAY) {
			goto LBL_M;
		}

		/* automatically pick the comba one if available (saves quite a few calls/ifs) */
		if (can_use_fast_column_array(P->used + P->used + 1, P->used)) {
			redux = fast_mp_montgomery_reduce;
		} else {
			/* use slower baseline Montgomery method */
			redux = mp_montgomery_reduce;
		}
	} else if (redmode == 1) {
		/* setup DR reduction for moduli of the form B**k - b */
		diminished_radix_setup(P, &mp);
		redux = diminished_radix_reduce;
	} else {
		/* setup DR reduction for moduli of the form 2**k - b */
		if ((err = mp_reduce_2k_setup(P, &mp)) != MP_OKAY) {
			goto LBL_M;
		}
		redux = mp_reduce_2k;
	}

	/* setup result */
	if ((err = mp_init(&res)) != MP_OKAY) {
		goto LBL_M;
	}

	/* create M table
	*

	*
	* The first half of the table is not computed though accept for M[0] and M[1]
	*/

	if (redmode == 0) {
		/* now we need R mod m */
		if ((err = mp_montgomery_calc_normalization(&res, P)) != MP_OKAY) {
			goto LBL_RES;
		}

		/* now set M[1] to G * R mod m */
		if ((err = multiply_modulo(&M[1], G, &res, P)) != MP_OKAY) {
			goto LBL_RES;
		}
	} else {
		set_word(&res, 1);
		if ((err = modulo(G, P, &M[1])) != MP_OKAY) {
			goto LBL_RES;
		}
	}

	/* compute the value at M[1<<(winsize-1)] by squaring M[1] (winsize-1) times */
	if ((err = mp_copy( &M[1], &M[1 << (winsize - 1)])) != MP_OKAY) {
		goto LBL_RES;
	}

	for (x = 0; x < (winsize - 1); x++) {
		if ((err = square(&M[1 << (winsize - 1)], &M[1 << (winsize - 1)])) != MP_OKAY) {
			goto LBL_RES;
		}
		if ((err = (*redux)(&M[1 << (winsize - 1)], P, mp)) != MP_OKAY) {
			goto LBL_RES;
		}
	}

	/* create upper table */
	for (x = (1 << (winsize - 1)) + 1; x < (1 << winsize); x++) {
		if ((err = signed_multiply(&M[x - 1], &M[1], &M[x])) != MP_OKAY) {
			goto LBL_RES;
		}
		if ((err = (*redux)(&M[x], P, mp)) != MP_OKAY) {
			goto LBL_RES;
		}
	}

	/* set initial mode and bit cnt */
	mode = 0;
	bitcnt = 1;
	buf = 0;
	digidx = X->used - 1;
	bitcpy = 0;
	bitbuf = 0;

	for (;;) {
		/* grab next digit as required */
		if (--bitcnt == 0) {
			/* if digidx == -1 we are out of digits so break */
			if (digidx == -1) {
				break;
			}
			/* read next digit and reset bitcnt */
			buf = X->dp[digidx--];
			bitcnt = (int)DIGIT_BIT;
		}

		/* grab the next msb from the exponent */
		y = (int)(mp_digit)((mp_digit)buf >> (unsigned)(DIGIT_BIT - 1)) & 1;
		buf <<= (mp_digit)1;

		/* if the bit is zero and mode == 0 then we ignore it
		* These represent the leading zero bits before the first 1 bit
		* in the exponent.  Technically this opt is not required but it
		* does lower the # of trivial squaring/reductions used
		*/
		if (mode == 0 && y == 0) {
			continue;
		}

		/* if the bit is zero and mode == 1 then we square */
		if (mode == 1 && y == 0) {
			if ((err = square(&res, &res)) != MP_OKAY) {
				goto LBL_RES;
			}
			if ((err = (*redux)(&res, P, mp)) != MP_OKAY) {
				goto LBL_RES;
			}
			continue;
		}

		/* else we add it to the window */
		bitbuf |= (y << (winsize - ++bitcpy));
		mode = 2;

		if (bitcpy == winsize) {
			/* ok window is filled so square as required and multiply  */
			/* square first */
			for (x = 0; x < winsize; x++) {
				if ((err = square(&res, &res)) != MP_OKAY) {
					goto LBL_RES;
				}
				if ((err = (*redux)(&res, P, mp)) != MP_OKAY) {
					goto LBL_RES;
				}
			}

			/* then multiply */
			if ((err = signed_multiply(&res, &M[bitbuf], &res)) != MP_OKAY) {
				goto LBL_RES;
			}
			if ((err = (*redux)(&res, P, mp)) != MP_OKAY) {
				goto LBL_RES;
			}

			/* empty window and reset */
			bitcpy = 0;
			bitbuf = 0;
			mode = 1;
		}
	}

	/* if bits remain then square/multiply */
	if (mode == 2 && bitcpy > 0) {
		/* square then multiply if the bit is set */
		for (x = 0; x < bitcpy; x++) {
			if ((err = square(&res, &res)) != MP_OKAY) {
				goto LBL_RES;
			}
			if ((err = (*redux)(&res, P, mp)) != MP_OKAY) {
				goto LBL_RES;
			}

			/* get next bit of the window */
			bitbuf <<= 1;
			if ((bitbuf & (1 << winsize)) != 0) {
				/* then multiply */
				if ((err = signed_multiply(&res, &M[1], &res)) != MP_OKAY) {
					goto LBL_RES;
				}
				if ((err = (*redux)(&res, P, mp)) != MP_OKAY) {
					goto LBL_RES;
				}
			}
		}
	}

	if (redmode == 0) {
		/* fixup result if Montgomery reduction is used
		* recall that any value in a Montgomery system is
		* actually multiplied by R mod n.  So we have
		* to reduce one more time to cancel out the factor
		* of R.
		*/
		if ((err = (*redux)(&res, P, mp)) != MP_OKAY) {
			goto LBL_RES;
		}
	}

	/* swap res with Y */
	mp_exch(&res, Y);
	err = MP_OKAY;
LBL_RES:
	mp_clear(&res);
LBL_M:
	mp_clear(&M[1]);
	for (x = 1<<(winsize-1); x < (1 << winsize); x++) {
		mp_clear(&M[x]);
	}
	return err;
}

/* Source: /usr/cvsroot/libtommath/dist/libtommath/bn_fast_exponent_modulo.c,v $ */
/* Revision: 1.4 $ */
/* Date: 2011/03/18 16:43:04 $ */

/* this is a shell function that calls either the normal or Montgomery
 * exptmod functions.  Originally the call to the montgomery code was
 * embedded in the normal function but that wasted alot of stack space
 * for nothing (since 99% of the time the Montgomery code would be called)
 */
static int
exponent_modulo(mp_int * G, mp_int * X, mp_int * P, mp_int *Y)
{
	int diminished_radix;

	/* modulus P must be positive */
	if (P->sign == MP_NEG) {
		return MP_VAL;
	}

	/* if exponent X is negative we have to recurse */
	if (X->sign == MP_NEG) {
		mp_int tmpG, tmpX;
		int err;

		/* first compute 1/G mod P */
		if ((err = mp_init(&tmpG)) != MP_OKAY) {
			return err;
		}
		if ((err = modular_inverse(&tmpG, G, P)) != MP_OKAY) {
			mp_clear(&tmpG);
			return err;
		}

		/* now get |X| */
		if ((err = mp_init(&tmpX)) != MP_OKAY) {
			mp_clear(&tmpG);
			return err;
		}
		if ((err = absolute(X, &tmpX)) != MP_OKAY) {
			mp_clear_multi(&tmpG, &tmpX, NULL);
			return err;
		}

		/* and now compute (1/G)**|X| instead of G**X [X < 0] */
		err = exponent_modulo(&tmpG, &tmpX, P, Y);
		mp_clear_multi(&tmpG, &tmpX, NULL);
		return err;
	}

	/* modified diminished radix reduction */
	if (mp_reduce_is_2k_l(P) == MP_YES) {
		return basic_exponent_mod(G, X, P, Y, 1);
	}

	/* is it a DR modulus? */
	diminished_radix = is_diminished_radix_modulus(P);

	/* if not, is it a unrestricted DR modulus? */
	if (!diminished_radix) {
		diminished_radix = mp_reduce_is_2k(P) << 1;
	}

	/* if the modulus is odd or diminished_radix, use the montgomery method */
	if (BN_is_odd(P) == 1 || diminished_radix) {
		return fast_exponent_modulo(G, X, P, Y, diminished_radix);
	}
	/* otherwise use the generic Barrett reduction technique */
	return basic_exponent_mod(G, X, P, Y, 0);
}

/* reverse an array, used for radix code */
static void
bn_reverse(unsigned char *s, int len)
{
	int     ix, iy;
	uint8_t t;

	for (ix = 0, iy = len - 1; ix < iy ; ix++, --iy) {
		t = s[ix];
		s[ix] = s[iy];
		s[iy] = t;
	}
}

static inline int
is_power_of_two(mp_digit b, int *p)
{
	int x;

	/* fast return if no power of two */
	if ((b==0) || (b & (b-1))) {
		return 0;
	}

	for (x = 0; x < DIGIT_BIT; x++) {
		if (b == (((mp_digit)1)<<x)) {
			*p = x;
			return 1;
		}
	}
	return 0;
}

/* single digit division (based on routine from MPI) */
static int
signed_divide_word(mp_int *a, mp_digit b, mp_int *c, mp_digit *d)
{
	mp_int  q;
	mp_word w;
	mp_digit t;
	int     res, ix;

	/* cannot divide by zero */
	if (b == 0) {
		return MP_VAL;
	}

	/* quick outs */
	if (b == 1 || MP_ISZERO(a) == 1) {
		if (d != NULL) {
			*d = 0;
		}
		if (c != NULL) {
			return mp_copy(a, c);
		}
		return MP_OKAY;
	}

	/* power of two ? */
	if (is_power_of_two(b, &ix) == 1) {
		if (d != NULL) {
			*d = a->dp[0] & ((((mp_digit)1)<<ix) - 1);
		}
		if (c != NULL) {
			return rshift_bits(a, ix, c, NULL);
		}
		return MP_OKAY;
	}

	/* three? */
	if (b == 3) {
		return third(a, c, d);
	}

	/* no easy answer [c'est la vie].  Just division */
	if ((res = mp_init_size(&q, a->used)) != MP_OKAY) {
		return res;
	}

	q.used = a->used;
	q.sign = a->sign;
	w = 0;
	for (ix = a->used - 1; ix >= 0; ix--) {
		w = (w << ((mp_word)DIGIT_BIT)) | ((mp_word)a->dp[ix]);

		if (w >= b) {
			t = (mp_digit)(w / b);
			w -= ((mp_word)t) * ((mp_word)b);
		} else {
			t = 0;
		}
		q.dp[ix] = (mp_digit)t;
	}

	if (d != NULL) {
		*d = (mp_digit)w;
	}

	if (c != NULL) {
		trim_unused_digits(&q);
		mp_exch(&q, c);
	}
	mp_clear(&q);

	return res;
}

static const mp_digit ltm_prime_tab[] = {
	0x0002, 0x0003, 0x0005, 0x0007, 0x000B, 0x000D, 0x0011, 0x0013,
	0x0017, 0x001D, 0x001F, 0x0025, 0x0029, 0x002B, 0x002F, 0x0035,
	0x003B, 0x003D, 0x0043, 0x0047, 0x0049, 0x004F, 0x0053, 0x0059,
	0x0061, 0x0065, 0x0067, 0x006B, 0x006D, 0x0071, 0x007F,
#ifndef MP_8BIT
	0x0083,
	0x0089, 0x008B, 0x0095, 0x0097, 0x009D, 0x00A3, 0x00A7, 0x00AD,
	0x00B3, 0x00B5, 0x00BF, 0x00C1, 0x00C5, 0x00C7, 0x00D3, 0x00DF,
	0x00E3, 0x00E5, 0x00E9, 0x00EF, 0x00F1, 0x00FB, 0x0101, 0x0107,
	0x010D, 0x010F, 0x0115, 0x0119, 0x011B, 0x0125, 0x0133, 0x0137,

	0x0139, 0x013D, 0x014B, 0x0151, 0x015B, 0x015D, 0x0161, 0x0167,
	0x016F, 0x0175, 0x017B, 0x017F, 0x0185, 0x018D, 0x0191, 0x0199,
	0x01A3, 0x01A5, 0x01AF, 0x01B1, 0x01B7, 0x01BB, 0x01C1, 0x01C9,
	0x01CD, 0x01CF, 0x01D3, 0x01DF, 0x01E7, 0x01EB, 0x01F3, 0x01F7,
	0x01FD, 0x0209, 0x020B, 0x021D, 0x0223, 0x022D, 0x0233, 0x0239,
	0x023B, 0x0241, 0x024B, 0x0251, 0x0257, 0x0259, 0x025F, 0x0265,
	0x0269, 0x026B, 0x0277, 0x0281, 0x0283, 0x0287, 0x028D, 0x0293,
	0x0295, 0x02A1, 0x02A5, 0x02AB, 0x02B3, 0x02BD, 0x02C5, 0x02CF,

	0x02D7, 0x02DD, 0x02E3, 0x02E7, 0x02EF, 0x02F5, 0x02F9, 0x0301,
	0x0305, 0x0313, 0x031D, 0x0329, 0x032B, 0x0335, 0x0337, 0x033B,
	0x033D, 0x0347, 0x0355, 0x0359, 0x035B, 0x035F, 0x036D, 0x0371,
	0x0373, 0x0377, 0x038B, 0x038F, 0x0397, 0x03A1, 0x03A9, 0x03AD,
	0x03B3, 0x03B9, 0x03C7, 0x03CB, 0x03D1, 0x03D7, 0x03DF, 0x03E5,
	0x03F1, 0x03F5, 0x03FB, 0x03FD, 0x0407, 0x0409, 0x040F, 0x0419,
	0x041B, 0x0425, 0x0427, 0x042D, 0x043F, 0x0443, 0x0445, 0x0449,
	0x044F, 0x0455, 0x045D, 0x0463, 0x0469, 0x047F, 0x0481, 0x048B,

	0x0493, 0x049D, 0x04A3, 0x04A9, 0x04B1, 0x04BD, 0x04C1, 0x04C7,
	0x04CD, 0x04CF, 0x04D5, 0x04E1, 0x04EB, 0x04FD, 0x04FF, 0x0503,
	0x0509, 0x050B, 0x0511, 0x0515, 0x0517, 0x051B, 0x0527, 0x0529,
	0x052F, 0x0551, 0x0557, 0x055D, 0x0565, 0x0577, 0x0581, 0x058F,
	0x0593, 0x0595, 0x0599, 0x059F, 0x05A7, 0x05AB, 0x05AD, 0x05B3,
	0x05BF, 0x05C9, 0x05CB, 0x05CF, 0x05D1, 0x05D5, 0x05DB, 0x05E7,
	0x05F3, 0x05FB, 0x0607, 0x060D, 0x0611, 0x0617, 0x061F, 0x0623,
	0x062B, 0x062F, 0x063D, 0x0641, 0x0647, 0x0649, 0x064D, 0x0653
#endif
};

#define PRIME_SIZE	__arraycount(ltm_prime_tab)

static inline int
mp_prime_is_divisible(mp_int *a, int *result)
{
	int     err, ix;
	mp_digit res;

	/* default to not */
	*result = MP_NO;

	for (ix = 0; ix < (int)PRIME_SIZE; ix++) {
		/* what is a mod LBL_prime_tab[ix] */
		if ((err = signed_divide_word(a, ltm_prime_tab[ix], NULL, &res)) != MP_OKAY) {
			return err;
		}

		/* is the residue zero? */
		if (res == 0) {
			*result = MP_YES;
			return MP_OKAY;
		}
	}

	return MP_OKAY;
}

/* single digit addition */
static int
add_single_digit(mp_int *a, mp_digit b, mp_int *c)
{
	int     res, ix, oldused;
	mp_digit *tmpa, *tmpc, mu;

	/* grow c as required */
	if (c->alloc < a->used + 1) {
		if ((res = mp_grow(c, a->used + 1)) != MP_OKAY) {
			return res;
		}
	}

	/* if a is negative and |a| >= b, call c = |a| - b */
	if (a->sign == MP_NEG && (a->used > 1 || a->dp[0] >= b)) {
		/* temporarily fix sign of a */
		a->sign = MP_ZPOS;

		/* c = |a| - b */
		res = signed_subtract_word(a, b, c);

		/* fix sign  */
		a->sign = c->sign = MP_NEG;

		/* clamp */
		trim_unused_digits(c);

		return res;
	}

	/* old number of used digits in c */
	oldused = c->used;

	/* sign always positive */
	c->sign = MP_ZPOS;

	/* source alias */
	tmpa = a->dp;

	/* destination alias */
	tmpc = c->dp;

	/* if a is positive */
	if (a->sign == MP_ZPOS) {
		/* add digit, after this we're propagating
		* the carry.
		*/
		*tmpc = *tmpa++ + b;
		mu = *tmpc >> DIGIT_BIT;
		*tmpc++ &= MP_MASK;

		/* now handle rest of the digits */
		for (ix = 1; ix < a->used; ix++) {
			*tmpc = *tmpa++ + mu;
			mu = *tmpc >> DIGIT_BIT;
			*tmpc++ &= MP_MASK;
		}
		/* set final carry */
		ix++;
		*tmpc++  = mu;

		/* setup size */
		c->used = a->used + 1;
	} else {
		/* a was negative and |a| < b */
		c->used  = 1;

		/* the result is a single digit */
		if (a->used == 1) {
			*tmpc++  =  b - a->dp[0];
		} else {
			*tmpc++  =  b;
		}

		/* setup count so the clearing of oldused
		* can fall through correctly
		*/
		ix = 1;
	}

	/* now zero to oldused */
	while (ix++ < oldused) {
		*tmpc++ = 0;
	}
	trim_unused_digits(c);

	return MP_OKAY;
}

/* single digit subtraction */
static int
signed_subtract_word(mp_int *a, mp_digit b, mp_int *c)
{
	mp_digit *tmpa, *tmpc, mu;
	int       res, ix, oldused;

	/* grow c as required */
	if (c->alloc < a->used + 1) {
		if ((res = mp_grow(c, a->used + 1)) != MP_OKAY) {
			return res;
		}
	}

	/* if a is negative just do an unsigned
	* addition [with fudged signs]
	*/
	if (a->sign == MP_NEG) {
		a->sign = MP_ZPOS;
		res = add_single_digit(a, b, c);
		a->sign = c->sign = MP_NEG;

		/* clamp */
		trim_unused_digits(c);

		return res;
	}

	/* setup regs */
	oldused = c->used;
	tmpa = a->dp;
	tmpc = c->dp;

	/* if a <= b simply fix the single digit */
	if ((a->used == 1 && a->dp[0] <= b) || a->used == 0) {
		if (a->used == 1) {
			*tmpc++ = b - *tmpa;
		} else {
			*tmpc++ = b;
		}
		ix = 1;

		/* negative/1digit */
		c->sign = MP_NEG;
		c->used = 1;
	} else {
		/* positive/size */
		c->sign = MP_ZPOS;
		c->used = a->used;

		/* subtract first digit */
		*tmpc = *tmpa++ - b;
		mu = *tmpc >> (sizeof(mp_digit) * CHAR_BIT - 1);
		*tmpc++ &= MP_MASK;

		/* handle rest of the digits */
		for (ix = 1; ix < a->used; ix++) {
			*tmpc = *tmpa++ - mu;
			mu = *tmpc >> (sizeof(mp_digit) * CHAR_BIT - 1);
			*tmpc++ &= MP_MASK;
		}
	}

	/* zero excess digits */
	while (ix++ < oldused) {
		*tmpc++ = 0;
	}
	trim_unused_digits(c);
	return MP_OKAY;
}

static const int lnz[16] = { 
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
};

/* Counts the number of lsbs which are zero before the first zero bit */
static int
mp_cnt_lsb(mp_int *a)
{
	int x;
	mp_digit q, qq;

	/* easy out */
	if (MP_ISZERO(a) == 1) {
		return 0;
	}

	/* scan lower digits until non-zero */
	for (x = 0; x < a->used && a->dp[x] == 0; x++) {
	}
	q = a->dp[x];
	x *= DIGIT_BIT;

	/* now scan this digit until a 1 is found */
	if ((q & 1) == 0) {
		do {
			 qq  = q & 15;
			 /* LINTED previous op ensures range of qq */
			 x  += lnz[qq];
			 q >>= 4;
		} while (qq == 0);
	}
	return x;
}

/* c = a * a (mod b) */
static int
square_modulo(mp_int *a, mp_int *b, mp_int *c)
{
	int     res;
	mp_int  t;

	if ((res = mp_init(&t)) != MP_OKAY) {
		return res;
	}

	if ((res = square(a, &t)) != MP_OKAY) {
		mp_clear(&t);
		return res;
	}
	res = modulo(&t, b, c);
	mp_clear(&t);
	return res;
}

static int
mp_prime_miller_rabin(mp_int *a, mp_int *b, int *result)
{
	mp_int  n1, y, r;
	int     s, j, err;

	/* default */
	*result = MP_NO;

	/* ensure b > 1 */
	if (compare_digit(b, 1) != MP_GT) {
		return MP_VAL;
	}     

	/* get n1 = a - 1 */
	if ((err = mp_init_copy(&n1, a)) != MP_OKAY) {
		return err;
	}
	if ((err = signed_subtract_word(&n1, 1, &n1)) != MP_OKAY) {
		goto LBL_N1;
	}

	/* set 2**s * r = n1 */
	if ((err = mp_init_copy(&r, &n1)) != MP_OKAY) {
		goto LBL_N1;
	}

	/* count the number of least significant bits
	* which are zero
	*/
	s = mp_cnt_lsb(&r);

	/* now divide n - 1 by 2**s */
	if ((err = rshift_bits(&r, s, &r, NULL)) != MP_OKAY) {
		goto LBL_R;
	}

	/* compute y = b**r mod a */
	if ((err = mp_init(&y)) != MP_OKAY) {
		goto LBL_R;
	}
	if ((err = exponent_modulo(b, &r, a, &y)) != MP_OKAY) {
		goto LBL_Y;
	}

	/* if y != 1 and y != n1 do */
	if (compare_digit(&y, 1) != MP_EQ && signed_compare(&y, &n1) != MP_EQ) {
		j = 1;
		/* while j <= s-1 and y != n1 */
		while ((j <= (s - 1)) && signed_compare(&y, &n1) != MP_EQ) {
			if ((err = square_modulo(&y, a, &y)) != MP_OKAY) {
				goto LBL_Y;
			}

			/* if y == 1 then composite */
			if (compare_digit(&y, 1) == MP_EQ) {
				goto LBL_Y;
			}

			++j;
		}

		/* if y != n1 then composite */
		if (signed_compare(&y, &n1) != MP_EQ) {
			goto LBL_Y;
		}
	}

	/* probably prime now */
	*result = MP_YES;
LBL_Y:
	mp_clear(&y);
LBL_R:
	mp_clear(&r);
LBL_N1:
	mp_clear(&n1);
	return err;
}

/* performs a variable number of rounds of Miller-Rabin
 *
 * Probability of error after t rounds is no more than

 *
 * Sets result to 1 if probably prime, 0 otherwise
 */
static int
mp_prime_is_prime(mp_int *a, int t, int *result)
{
	mp_int  b;
	int     ix, err, res;

	/* default to no */
	*result = MP_NO;

	/* valid value of t? */
	if (t <= 0 || t > (int)PRIME_SIZE) {
		return MP_VAL;
	}

	/* is the input equal to one of the primes in the table? */
	for (ix = 0; ix < (int)PRIME_SIZE; ix++) {
		if (compare_digit(a, ltm_prime_tab[ix]) == MP_EQ) {
			*result = 1;
			return MP_OKAY;
		}
	}

	/* first perform trial division */
	if ((err = mp_prime_is_divisible(a, &res)) != MP_OKAY) {
		return err;
	}

	/* return if it was trivially divisible */
	if (res == MP_YES) {
		return MP_OKAY;
	}

	/* now perform the miller-rabin rounds */
	if ((err = mp_init(&b)) != MP_OKAY) {
		return err;
	}

	for (ix = 0; ix < t; ix++) {
		/* set the prime */
		set_word(&b, ltm_prime_tab[ix]);

		if ((err = mp_prime_miller_rabin(a, &b, &res)) != MP_OKAY) {
			goto LBL_B;
		}

		if (res == MP_NO) {
			goto LBL_B;
		}
	}

	/* passed the test */
	*result = MP_YES;
LBL_B:
	mp_clear(&b);
	return err;
}

/* returns size of ASCII reprensentation */
static int
mp_radix_size(mp_int *a, int radix, int *size)
{
	int     res, digs;
	mp_int  t;
	mp_digit d;

	*size = 0;

	/* special case for binary */
	if (radix == 2) {
		*size = mp_count_bits(a) + (a->sign == MP_NEG ? 1 : 0) + 1;
		return MP_OKAY;
	}

	/* make sure the radix is in range */
	if (radix < 2 || radix > 64) {
		return MP_VAL;
	}

	if (MP_ISZERO(a) == MP_YES) {
		*size = 2;
		return MP_OKAY;
	}

	/* digs is the digit count */
	digs = 0;

	/* if it's negative add one for the sign */
	if (a->sign == MP_NEG) {
		++digs;
	}

	/* init a copy of the input */
	if ((res = mp_init_copy(&t, a)) != MP_OKAY) {
		return res;
	}

	/* force temp to positive */
	t.sign = MP_ZPOS; 

	/* fetch out all of the digits */
	while (MP_ISZERO(&t) == MP_NO) {
		if ((res = signed_divide_word(&t, (mp_digit) radix, &t, &d)) != MP_OKAY) {
			mp_clear(&t);
			return res;
		}
		++digs;
	}
	mp_clear(&t);

	/* return digs + 1, the 1 is for the NULL byte that would be required. */
	*size = digs + 1;
	return MP_OKAY;
}

static const char *mp_s_rmap = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz+/";

/* stores a bignum as a ASCII string in a given radix (2..64) 
 *
 * Stores upto maxlen-1 chars and always a NULL byte 
 */
static int
mp_toradix_n(mp_int * a, char *str, int radix, int maxlen)
{
	int     res, digs;
	mp_int  t;
	mp_digit d;
	char   *_s = str;

	/* check range of the maxlen, radix */
	if (maxlen < 2 || radix < 2 || radix > 64) {
		return MP_VAL;
	}

	/* quick out if its zero */
	if (MP_ISZERO(a) == MP_YES) {
		*str++ = '0';
		*str = '\0';
		return MP_OKAY;
	}

	if ((res = mp_init_copy(&t, a)) != MP_OKAY) {
		return res;
	}

	/* if it is negative output a - */
	if (t.sign == MP_NEG) {
		/* we have to reverse our digits later... but not the - sign!! */
		++_s;

		/* store the flag and mark the number as positive */
		*str++ = '-';
		t.sign = MP_ZPOS;

		/* subtract a char */
		--maxlen;
	}

	digs = 0;
	while (MP_ISZERO(&t) == 0) {
		if (--maxlen < 1) {
			/* no more room */
			break;
		}
		if ((res = signed_divide_word(&t, (mp_digit) radix, &t, &d)) != MP_OKAY) {
			mp_clear(&t);
			return res;
		}
		/* LINTED -- radix' range is checked above, limits d's range */
		*str++ = mp_s_rmap[d];
		++digs;
	}

	/* reverse the digits of the string.  In this case _s points
	* to the first digit [exluding the sign] of the number
	*/
	bn_reverse((unsigned char *)_s, digs);

	/* append a NULL so the string is properly terminated */
	*str = '\0';

	mp_clear(&t);
	return MP_OKAY;
}

static char *
formatbn(const BIGNUM *a, const int radix)
{
	char	*s;
	int	 len;

	if (mp_radix_size(__UNCONST(a), radix, &len) != MP_OKAY) {
		return NULL;
	}
	if ((s = allocate(1, (size_t)len)) != NULL) {
		if (mp_toradix_n(__UNCONST(a), s, radix, len) != MP_OKAY) {
			deallocate(s, (size_t)len);
			return NULL;
		}
	}
	return s;
}

static int
mp_getradix_num(mp_int *a, int radix, char *s)
{
	int err, ch, neg, y;

	/* clear a */
	mp_zero(a);

	/* if first digit is - then set negative */
	if ((ch = *s++) == '-') {
		neg = MP_NEG;
		ch = *s++;
	} else {
		neg = MP_ZPOS;
	}

	for (;;) {
		/* find y in the radix map */
		for (y = 0; y < radix; y++) {
			if (mp_s_rmap[y] == ch) {
				break;
			}
		}
		if (y == radix) {
			break;
		}

		/* shift up and add */
		if ((err = multiply_digit(a, radix, a)) != MP_OKAY) {
			return err;
		}
		if ((err = add_single_digit(a, y, a)) != MP_OKAY) {
			return err;
		}

		ch = *s++;
	}
	if (compare_digit(a, 0) != MP_EQ) {
		a->sign = neg;
	}

	return MP_OKAY;
}

static int
getbn(BIGNUM **a, const char *str, int radix)
{
	int	len;

	if (a == NULL || str == NULL || (*a = BN_new()) == NULL) {
		return 0;
	}
	if (mp_getradix_num(*a, radix, __UNCONST(str)) != MP_OKAY) {
		return 0;
	}
	mp_radix_size(__UNCONST(*a), radix, &len);
	return len - 1;
}

/* d = a - b (mod c) */
static int
subtract_modulo(mp_int *a, mp_int *b, mp_int *c, mp_int *d)
{
	int     res;
	mp_int  t;


	if ((res = mp_init(&t)) != MP_OKAY) {
		return res;
	}

	if ((res = signed_subtract(a, b, &t)) != MP_OKAY) {
		mp_clear(&t);
		return res;
	}
	res = modulo(&t, c, d);
	mp_clear(&t);
	return res;
}

/**************************************************************************/

/* BIGNUM emulation layer */

/* essentiually, these are just wrappers around the libtommath functions */
/* usually the order of args changes */
/* the BIGNUM API tends to have more const poisoning */
/* these wrappers also check the arguments passed for sanity */

BIGNUM *
BN_bin2bn(const uint8_t *data, int len, BIGNUM *ret)
{
	if (data == NULL) {
		return BN_new();
	}
	if (ret == NULL) {
		ret = BN_new();
	}
	return (mp_read_unsigned_bin(ret, data, len) == MP_OKAY) ? ret : NULL;
}

/* store in unsigned [big endian] format */
int
BN_bn2bin(const BIGNUM *a, unsigned char *b)
{
	BIGNUM	t;
	int    	x;

	if (a == NULL || b == NULL) {
		return -1;
	}
	if (mp_init_copy (&t, __UNCONST(a)) != MP_OKAY) {
		return -1;
	}
	for (x = 0; !BN_is_zero(&t) ; ) {
		b[x++] = (unsigned char) (t.dp[0] & 0xff);
		if (rshift_bits(&t, 8, &t, NULL) != MP_OKAY) {
			mp_clear(&t);
			return -1;
		}
	}
	bn_reverse(b, x);
	mp_clear(&t);
	return x;
}

void
BN_init(BIGNUM *a)
{
	if (a != NULL) {
		mp_init(a);
	}
}

BIGNUM *
BN_new(void)
{
	BIGNUM	*a;

	if ((a = allocate(1, sizeof(*a))) != NULL) {
		mp_init(a);
	}
	return a;
}

/* copy, b = a */
int
BN_copy(BIGNUM *b, const BIGNUM *a)
{
	if (a == NULL || b == NULL) {
		return MP_VAL;
	}
	return mp_copy(__UNCONST(a), b);
}

BIGNUM *
BN_dup(const BIGNUM *a)
{
	BIGNUM	*ret;

	if (a == NULL) {
		return NULL;
	}
	if ((ret = BN_new()) != NULL) {
		BN_copy(ret, a);
	}
	return ret;
}

void
BN_swap(BIGNUM *a, BIGNUM *b)
{
	if (a && b) {
		mp_exch(a, b);
	}
}

int
BN_lshift(BIGNUM *r, const BIGNUM *a, int n)
{
	if (r == NULL || a == NULL || n < 0) {
		return 0;
	}
	BN_copy(r, a);
	return lshift_digits(r, n) == MP_OKAY;
}

int
BN_lshift1(BIGNUM *r, BIGNUM *a)
{
	if (r == NULL || a == NULL) {
		return 0;
	}
	BN_copy(r, a);
	return lshift_digits(r, 1) == MP_OKAY;
}

int
BN_rshift(BIGNUM *r, const BIGNUM *a, int n)
{
	if (r == NULL || a == NULL || n < 0) {
		return MP_VAL;
	}
	BN_copy(r, a);
	return rshift_digits(r, n) == MP_OKAY;
}

int
BN_rshift1(BIGNUM *r, BIGNUM *a)
{
	if (r == NULL || a == NULL) {
		return 0;
	}
	BN_copy(r, a);
	return rshift_digits(r, 1) == MP_OKAY;
}

int
BN_set_word(BIGNUM *a, BN_ULONG w)
{
	if (a == NULL) {
		return 0;
	}
	set_word(a, w);
	return 1;
}

int
BN_add(BIGNUM *r, const BIGNUM *a, const BIGNUM *b)
{
	if (a == NULL || b == NULL || r == NULL) {
		return 0;
	}
	return signed_add(__UNCONST(a), __UNCONST(b), r) == MP_OKAY;
}

int
BN_sub(BIGNUM *r, const BIGNUM *a, const BIGNUM *b)
{
	if (a == NULL || b == NULL || r == NULL) {
		return 0;
	}
	return signed_subtract(__UNCONST(a), __UNCONST(b), r) == MP_OKAY;
}

int
BN_mul(BIGNUM *r, const BIGNUM *a, const BIGNUM *b, BN_CTX *ctx)
{
	if (a == NULL || b == NULL || r == NULL) {
		return 0;
	}
	USE_ARG(ctx);
	return signed_multiply(__UNCONST(a), __UNCONST(b), r) == MP_OKAY;
}

int
BN_div(BIGNUM *dv, BIGNUM *rem, const BIGNUM *a, const BIGNUM *d, BN_CTX *ctx)
{
	if ((dv == NULL && rem == NULL) || a == NULL || d == NULL) {
		return 0;
	}
	USE_ARG(ctx);
	return signed_divide(dv, rem, __UNCONST(a), __UNCONST(d)) == MP_OKAY;
}

/* perform a bit operation on the 2 bignums */
int
BN_bitop(BIGNUM *r, const BIGNUM *a, char op, const BIGNUM *b)
{
	unsigned	ndigits;
	mp_digit	ad;
	mp_digit	bd;
	int		i;

	if (a == NULL || b == NULL || r == NULL) {
		return 0;
	}
	if (BN_cmp(__UNCONST(a), __UNCONST(b)) >= 0) {
		BN_copy(r, a);
		ndigits = a->used;
	} else {
		BN_copy(r, b);
		ndigits = b->used;
	}
	for (i = 0 ; i < (int)ndigits ; i++) {
		ad = (i > a->used) ? 0 : a->dp[i];
		bd = (i > b->used) ? 0 : b->dp[i];
		switch(op) {
		case '&':
			r->dp[i] = (ad & bd);
			break;
		case '|':
			r->dp[i] = (ad | bd);
			break;
		case '^':
			r->dp[i] = (ad ^ bd);
			break;
		default:
			break;
		}
	}
	return 1;
}

void
BN_free(BIGNUM *a)
{
	if (a) {
		mp_clear(a);
	}
}

void
BN_clear(BIGNUM *a)
{
	if (a) {
		mp_clear(a);
	}
}

void
BN_clear_free(BIGNUM *a)
{
	if (a) {
		mp_clear(a);
	}
}

int
BN_num_bytes(const BIGNUM *a)
{
	if (a == NULL) {
		return MP_VAL;
	}
	return mp_unsigned_bin_size(__UNCONST(a));
}

int
BN_num_bits(const BIGNUM *a)
{
	if (a == NULL) {
		return 0;
	}
	return mp_count_bits(a);
}

void
BN_set_negative(BIGNUM *a, int n)
{
	if (a) {
		a->sign = (n) ? MP_NEG : 0;
	}
}

int
BN_cmp(BIGNUM *a, BIGNUM *b)
{
	if (a == NULL || b == NULL) {
		return MP_VAL;
	}
	switch(signed_compare(a, b)) {
	case MP_LT:
		return -1;
	case MP_GT:
		return 1;
	case MP_EQ:
	default:
		return 0;
	}
}

int
BN_mod_exp(BIGNUM *Y, BIGNUM *G, BIGNUM *X, BIGNUM *P, BN_CTX *ctx)
{
	if (Y == NULL || G == NULL || X == NULL || P == NULL) {
		return MP_VAL;
	}
	USE_ARG(ctx);
	return exponent_modulo(G, X, P, Y) == MP_OKAY;
}

BIGNUM *
BN_mod_inverse(BIGNUM *r, BIGNUM *a, const BIGNUM *n, BN_CTX *ctx)
{
	USE_ARG(ctx);
	if (r == NULL || a == NULL || n == NULL) {
		return NULL;
	}
	return (modular_inverse(r, a, __UNCONST(n)) == MP_OKAY) ? r : NULL;
}

int
BN_mod_mul(BIGNUM *ret, BIGNUM *a, BIGNUM *b, const BIGNUM *m, BN_CTX *ctx)
{
	USE_ARG(ctx);
	if (ret == NULL || a == NULL || b == NULL || m == NULL) {
		return 0;
	}
	return multiply_modulo(ret, a, b, __UNCONST(m)) == MP_OKAY;
}

BN_CTX *
BN_CTX_new(void)
{
	return allocate(1, sizeof(BN_CTX));
}

void
BN_CTX_init(BN_CTX *c)
{
	if (c != NULL) {
		c->arraysize = 15;
		if ((c->v = allocate(sizeof(*c->v), c->arraysize)) == NULL) {
			c->arraysize = 0;
		}
	}
}

BIGNUM *
BN_CTX_get(BN_CTX *ctx)
{
	if (ctx == NULL || ctx->v == NULL || ctx->arraysize == 0 || ctx->count == ctx->arraysize - 1) {
		return NULL;
	}
	return ctx->v[ctx->count++] = BN_new();
}

void
BN_CTX_start(BN_CTX *ctx)
{
	BN_CTX_init(ctx);
}

void
BN_CTX_free(BN_CTX *c)
{
	unsigned	i;

	if (c != NULL && c->v != NULL) {
		for (i = 0 ; i < c->count ; i++) {
			BN_clear_free(c->v[i]);
		}
		deallocate(c->v, sizeof(*c->v) * c->arraysize);
	}
}

void
BN_CTX_end(BN_CTX *ctx)
{
	BN_CTX_free(ctx);
}

char *
BN_bn2hex(const BIGNUM *a)
{
	return (a == NULL) ? NULL : formatbn(a, 16);
}

char *
BN_bn2dec(const BIGNUM *a)
{
	return (a == NULL) ? NULL : formatbn(a, 10);
}

char *
BN_bn2radix(const BIGNUM *a, unsigned radix)
{
	return (a == NULL) ? NULL : formatbn(a, (int)radix);
}

#ifndef _KERNEL
int
BN_print_fp(FILE *fp, const BIGNUM *a)
{
	char	*s;
	int	 ret;

	if (fp == NULL || a == NULL) {
		return 0;
	}
	s = BN_bn2hex(a);
	ret = fprintf(fp, "%s", s);
	deallocate(s, strlen(s) + 1);
	return ret;
}
#endif

#ifdef BN_RAND_NEEDED
int
BN_rand(BIGNUM *rnd, int bits, int top, int bottom)
{
	uint64_t	r;
	int		digits;
	int		i;

	if (rnd == NULL) {
		return 0;
	}
	mp_init_size(rnd, digits = howmany(bits, DIGIT_BIT));
	for (i = 0 ; i < digits ; i++) {
		r = (uint64_t)arc4random();
		r <<= 32;
		r |= arc4random();
		rnd->dp[i] = (r & MP_MASK);
	}
	if (top == 0) {
		rnd->dp[rnd->used - 1] |= (((mp_digit)1)<<((mp_digit)DIGIT_BIT));
	}
	if (top == 1) {
		rnd->dp[rnd->used - 1] |= (((mp_digit)1)<<((mp_digit)DIGIT_BIT));
		rnd->dp[rnd->used - 1] |= (((mp_digit)1)<<((mp_digit)(DIGIT_BIT - 1)));
	}
	if (bottom) {
		rnd->dp[0] |= 0x1;
	}
	return 1;
}

int
BN_rand_range(BIGNUM *rnd, BIGNUM *range)
{
	if (rnd == NULL || range == NULL || BN_is_zero(range)) {
		return 0;
	}
	BN_rand(rnd, BN_num_bits(range), 1, 0);
	return modulo(rnd, range, rnd) == MP_OKAY;
}
#endif

int
BN_is_prime(const BIGNUM *a, int checks, void (*callback)(int, int, void *), BN_CTX *ctx, void *cb_arg)
{
	int	primality;

	if (a == NULL) {
		return 0;
	}
	USE_ARG(ctx);
	USE_ARG(cb_arg);
	USE_ARG(callback);
	return (mp_prime_is_prime(__UNCONST(a), checks, &primality) == MP_OKAY) ? primality : 0; 
}

const BIGNUM *
BN_value_one(void)
{
	static mp_digit		digit = 1UL;
	static const BIGNUM	one = { &digit, 1, 1, 0 };

	return &one;
}

int
BN_hex2bn(BIGNUM **a, const char *str)
{
	return getbn(a, str, 16);
}

int
BN_dec2bn(BIGNUM **a, const char *str)
{
	return getbn(a, str, 10);
}

int
BN_radix2bn(BIGNUM **a, const char *str, unsigned radix)
{
	return getbn(a, str, (int)radix);
}

int
BN_mod_sub(BIGNUM *r, BIGNUM *a, BIGNUM *b, const BIGNUM *m, BN_CTX *ctx)
{
	USE_ARG(ctx);
	if (r == NULL || a == NULL || b == NULL || m == NULL) {
		return 0;
	}
	return subtract_modulo(a, b, __UNCONST(m), r) == MP_OKAY;
}

int
BN_is_bit_set(const BIGNUM *a, int n)
{
	if (a == NULL || n < 0 || n >= a->used * DIGIT_BIT) {
		return 0;
	}
	return (a->dp[n / DIGIT_BIT] & (1 << (n % DIGIT_BIT))) ? 1 : 0;
}

/* raise 'a' to power of 'b' */
int
BN_raise(BIGNUM *res, BIGNUM *a, BIGNUM *b)
{
	uint64_t	 exponent;
	BIGNUM		*power;
	BIGNUM		*temp;
	char		*t;

	t = BN_bn2dec(b);
	exponent = (uint64_t)strtoull(t, NULL, 10);
	free(t);
	if (exponent == 0) {
		BN_copy(res, BN_value_one());
	} else {
		power = BN_dup(a);
		for ( ; (exponent & 1) == 0 ; exponent >>= 1) {
			BN_mul(power, power, power, NULL);
		}
		temp = BN_dup(power);
		for (exponent >>= 1 ; exponent > 0 ; exponent >>= 1) {
			BN_mul(power, power, power, NULL);
			if (exponent & 1) {
				BN_mul(temp, power, temp, NULL);
			}
		}
		BN_copy(res, temp);
		BN_free(power);
		BN_free(temp);
	}
	return 1;
}

/* compute the factorial */
int
BN_factorial(BIGNUM *res, BIGNUM *f)
{
	BIGNUM	*one;
	BIGNUM	*i;

	i = BN_dup(f);
	one = __UNCONST(BN_value_one());
	BN_sub(i, i, one);
	BN_copy(res, f);
	while (BN_cmp(i, one) > 0) {
		BN_mul(res, res, i, NULL);
		BN_sub(i, i, one);
	}
	BN_free(i);
	return 1;
}
