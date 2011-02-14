/*	$NetBSD: byte_swap_8.S,v 1.1 2010/01/14 02:09:46 joerg Exp $	*/

/*
 * Written by Frank van der Linden (fvdl@wasabisystems.com)
 * Public Domain.
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS)
	RCSID("$NetBSD: byte_swap_8.S,v 1.1 2010/01/14 02:09:46 joerg Exp $")
#endif

ENTRY(bswap64)
	bswapq	%rdi
	movq	%rdi,%rax
	ret
