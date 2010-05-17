#include <minix/u64.h>

u64_t mul64(u64_t i, u64_t j)
{
	u64_t result;

	/* Compute as follows:
	 *   i * j =
	 *   (i.hi << 32 + i.lo) * (j.hi << 32 + j.lo) =
	 *   (i.hi << 32) * (j.hi << 32 + j.lo) + i.lo * (j.hi << 32 + j.lo) =
	 *   (i.hi * j.hi) << 64 + (i.hi * j.lo) << 32 + (i.lo * j.hi << 32) + i.lo * j.lo
	 *
	 * 64-bit-result multiply only needed for (i.lo * j.lo)
	 * upper 32 bits overflow for (i.lo * j.hi) and (i.hi * j.lo)
	 * all overflows for (i.hi * j.hi)
	 */
	result = mul64u(i.lo, j.lo);
	result.hi += i.hi * j.lo + i.lo * j.hi;	
	return result;
}
