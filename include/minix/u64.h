/*	minix/u64.h					Author: Kees J. Bot
 *								7 Dec 1995
 * Functions to manipulate 64 bit disk addresses.
 */
#ifndef _MINIX__U64_H
#define _MINIX__U64_H

#include <sys/types.h>

#include <limits.h>

static inline int bsr64(u64_t i)
{
	int index;
	u64_t mask;

	for (index = 63, mask = 1ULL << 63; index >= 0; --index, mask >>= 1) {
	    if (i & mask)
		return index;
	}

	return -1;
}

static inline int cmp64(u64_t i, u64_t j)
{
	if (i > j)
		return 1;
	else if (i < j)
		return -1;
	else /* (i == j) */
		return 0;
}

static inline int cmp64u(u64_t i, unsigned j)
{
	if (i > j)
		return 1;
	else if (i < j)
		return -1;
	else /* (i == j) */
		return 0;
}

static inline int cmp64ul(u64_t i, unsigned long j)
{
	if (i > j)
		return 1;
	else if (i < j)
		return -1;
	else /* (i == j) */
		return 0;
}

static inline unsigned diff64(u64_t i, u64_t j)
{
	return (unsigned)(i - j);
}

static inline u64_t div64(u64_t i, u64_t j)
{
        return i / j;
}

static inline u64_t rem64(u64_t i, u64_t j)
{
	return i % j;
}

static inline unsigned long div64u(u64_t i, unsigned j)
{
	return (unsigned long)(i / j);
}

static inline u64_t div64u64(u64_t i, unsigned j)
{
	return i / j;
}

static inline unsigned rem64u(u64_t i, unsigned j)
{
	return (unsigned)(i % j);
}

static inline unsigned long ex64lo(u64_t i)
{
	return (unsigned long)i;
}

static inline unsigned long ex64hi(u64_t i)
{
	return (unsigned long)(i>>32);
}

static inline u64_t make64(unsigned long lo, unsigned long hi)
{
	return ((u64_t)hi << 32) | (u64_t)lo;
}

static inline u64_t mul64(u64_t i, u64_t j)
{
	return i * j;
}

static inline u64_t mul64u(unsigned long i, unsigned j)
{
	return (u64_t)i * j;
}

static inline u64_t sub64(u64_t i, u64_t j)
{
	return i - j;
}

static inline u64_t sub64u(u64_t i, unsigned j)
{
	return i - j;
}

static inline u64_t sub64ul(u64_t i, unsigned long j)
{
	return i - j;
}

u64_t rrotate64(u64_t x, unsigned short b);
u64_t rshift64(u64_t x, unsigned short b);
u64_t xor64(u64_t a, u64_t b);
u64_t and64(u64_t a, u64_t b);
u64_t not64(u64_t a);

#endif /* _MINIX__U64_H */
