/* Few u64 utils implemented in C
 * Author: Gautam BT
 */
#include <minix/u64.h>

u64_t rrotate64(u64_t x, unsigned short b)
{
	b %= 64;
	if ((b &= 63) == 0)
		return x;
	return (x >> b) | (x << (64 - b));
}

u64_t rshift64(u64_t x, unsigned short b)
{
	if (b >= 64)
		return 0;
	return x >> b;
}

u64_t xor64(u64_t a, u64_t b)
{
	return a ^ b;
}

u64_t and64(u64_t a, u64_t b)
{
	return a & b;
}

u64_t not64(u64_t a)
{
	return ~a;
}
