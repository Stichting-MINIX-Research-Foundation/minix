/*
 * This file implements 64-bit arithmentic functions. These functions will
 * go away once clang is ready.
 *
 * It will only work with GCC and clang.
 *
 */

#include <minix/u64.h>
#include <limits.h>

#if !defined(__LONG_LONG_SUPPORTED)
#error "ERROR: This file requires long long support"
#endif


u64_t add64(u64_t i, u64_t j)
{
	return i + j;
}

u64_t add64u(u64_t i, unsigned j)
{
	return i + j;
}

u64_t add64ul(u64_t i, unsigned long j)
{
	return i + j;
}

int bsr64(u64_t i)
{
	int index;
	u64_t mask;

	for (index = 63, mask = 1ULL << 63; index >= 0; --index, mask >>= 1) {
	    if (i & mask)
		return index;
	}

	return -1;
}

int cmp64(u64_t i, u64_t j)
{
	if (i > j)
		return 1;
	else if (i < j)
		return -1;
	else /* (i == j) */
		return 0;
}

int cmp64u(u64_t i, unsigned j)
{
	if (i > j)
		return 1;
	else if (i < j)
		return -1;
	else /* (i == j) */
		return 0;
}

int cmp64ul(u64_t i, unsigned long j)
{
	if (i > j)
		return 1;
	else if (i < j)
		return -1;
	else /* (i == j) */
		return 0;
}

unsigned cv64u(u64_t i)
{
/* return ULONG_MAX if really big */
    if (i>>32)
	return ULONG_MAX;

    return (unsigned)i;
}

unsigned long cv64ul(u64_t i)
{
/* return ULONG_MAX if really big */
    if (i>>32)
	return ULONG_MAX;

    return (unsigned long)i;
}

u64_t cvu64(unsigned i)
{
	return i;
}

u64_t cvul64(unsigned long i)
{
	return i;
}

unsigned diff64(u64_t i, u64_t j)
{
	return (unsigned)(i - j);
}

u64_t div64(u64_t i, u64_t j)
{
        return i / j;
}

u64_t rem64(u64_t i, u64_t j)
{
	return i % j;
}

unsigned long div64u(u64_t i, unsigned j)
{
	return (unsigned long)(i / j);
}

u64_t div64u64(u64_t i, unsigned j)
{
	return i / j;
}

unsigned rem64u(u64_t i, unsigned j)
{
	return (unsigned)(i % j);
}

unsigned long ex64lo(u64_t i)
{
	return (unsigned long)i;
}

unsigned long ex64hi(u64_t i)
{
	return (unsigned long)(i>>32);
}

u64_t make64(unsigned long lo, unsigned long hi)
{
	return ((u64_t)hi << 32) | (u64_t)lo;
}

u64_t mul64(u64_t i, u64_t j)
{
	return i * j;
}

u64_t mul64u(unsigned long i, unsigned j)
{
	return (u64_t)i * j;
}

u64_t sub64(u64_t i, u64_t j)
{
	return i - j;
}

u64_t sub64u(u64_t i, unsigned j)
{
	return i - j;
}

u64_t sub64ul(u64_t i, unsigned long j)
{
	return i - j;
}
