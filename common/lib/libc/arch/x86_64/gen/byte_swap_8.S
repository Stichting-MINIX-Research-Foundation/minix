/*	$NetBSD: byte_swap_8.S,v 1.2 2014/05/22 15:23:11 uebayasi Exp $	*/

/*
 * Written by Frank van der Linden (fvdl@wasabisystems.com)
 * Public Domain.
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS)
	RCSID("$NetBSD: byte_swap_8.S,v 1.2 2014/05/22 15:23:11 uebayasi Exp $")
#endif

ENTRY(bswap64)
	bswapq	%rdi
	movq	%rdi,%rax
	ret
END(bswap64)
