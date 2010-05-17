/*	div64() - full 64-bit division		                           */
/*	rem64() - full 64-bit modulo		                           */
/*						Author: Erik van der Kouwe */
/*						              14 May 2010  */
#include <assert.h>
#include <minix/u64.h>

static u32_t shl64hi(u64_t i, unsigned shift)
{
	/* compute the high-order 32-bit value in (i << shift) */
	if (shift == 0)
		return i.hi;
	else if (shift < 32)
		return (i.hi << shift) | (i.lo >> (32 - shift));
	else if (shift == 32)
		return i.lo;
	else if (shift < 64)
		return i.lo << (shift - 32);
	else
		return 0;		
}

static u64_t divrem64(u64_t *i, u64_t j)
{
	u32_t i32, j32, q;
	u64_t result = { 0, 0 };
	unsigned shift;

	assert(i);

	/* this function is not suitable for small divisors */
	assert(ex64hi(j) != 0);

	/* as long as i >= j we work on reducing i */
	while (cmp64(*i, j) >= 0) {
		/* shift to obtain the 32 most significant bits */
		shift = 63 - bsr64(*i);
		i32 = shl64hi(*i, shift);
		j32 = shl64hi(j, shift);

		/* find a lower bound for *i/j */
		if (j32 + 1 < j32) {
			/* avoid overflow, since *i >= j we know q >= 1 */
			q = 1; 
		} else {
			/* use 32-bit division, round j32 up to ensure that
			 * we obtain a lower bound 
			 */
			q = i32 / (j32 + 1);

			/* since *i >= j we know q >= 1 */
			if (q < 1) q = 1;
		}

		/* perform the division using the lower bound we found */
		*i = sub64(*i, mul64(j, cvu64(q)));
		result = add64u(result, q);
	}

	/* if we get here then *i < j; because we round down we are finished */
	return result;
}

u64_t div64(u64_t i, u64_t j)
{
	/* divrem64 is unsuitable for small divisors, especially zero which would 
	 * trigger a infinite loop; use assembly function in this case
	 */
	if (!ex64hi(j)) {
		return div64u64(i, ex64lo(j));
	}

	return divrem64(&i, j);
}

u64_t rem64(u64_t i, u64_t j)
{
	/* divrem64 is unsuitable for small divisors, especially zero which would 
	 * trigger a infinite loop; use assembly function in this case
	 */
	if (!ex64hi(j)) {
		return cvu64(rem64u(i, ex64lo(j)));
	}

	divrem64(&i, j);
	return i;
}
