/*	$NetBSD: byte_swap_2.S,v 1.2 2006/02/04 21:53:31 uwe Exp $	*/

#include <machine/asm.h>

#if defined(LIBC_SCCS)
	RCSID("$NetBSD: byte_swap_2.S,v 1.2 2006/02/04 21:53:31 uwe Exp $")
#endif

#if defined(_KERNEL) || defined(_STANDALONE)
_ENTRY(_C_LABEL(bswap16))
#else
_ENTRY(_C_LABEL(__bswap16))
#endif
_ENTRY(_C_LABEL(ntohs))
_ENTRY(_C_LABEL(htons))
_PROF_PROLOGUE
	movl	%edi,%eax
	xchgb	%ah,%al
	ret
