/*  $NetBSD: bswap64.c,v 1.3 2009/03/16 05:59:21 cegger Exp $    */

/*
 * Written by Manuel Bouyer <bouyer@NetBSD.org>.
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: bswap64.c,v 1.3 2009/03/16 05:59:21 cegger Exp $");
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <machine/bswap.h>

#undef bswap64

uint64_t
bswap64(uint64_t x)
{
#ifdef _LP64
	/*
	 * Assume we have wide enough registers to do it without touching
	 * memory.
	 */
	return  ( (x << 56) & 0xff00000000000000UL ) |
		( (x << 40) & 0x00ff000000000000UL ) |
		( (x << 24) & 0x0000ff0000000000UL ) |
		( (x <<  8) & 0x000000ff00000000UL ) |
		( (x >>  8) & 0x00000000ff000000UL ) |
		( (x >> 24) & 0x0000000000ff0000UL ) |
		( (x >> 40) & 0x000000000000ff00UL ) |
		( (x >> 56) & 0x00000000000000ffUL );
#else
	/*
	 * Split the operation in two 32bit steps.
	 */
	uint32_t tl, th;

	th = bswap32((uint32_t)(x & 0x00000000ffffffffULL));
	tl = bswap32((uint32_t)((x >> 32) & 0x00000000ffffffffULL));
	return ((uint64_t)th << 32) | tl;
#endif
}
