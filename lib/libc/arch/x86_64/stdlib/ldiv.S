/*	$NetBSD: ldiv.S,v 1.3 2014/05/22 15:01:56 uebayasi Exp $	*/

/*
 * Written by gcc 3.0.
 * Copy/pasted by Frank van der Linden (fvdl@wasabisystems.com)
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS)
	RCSID("$NetBSD: ldiv.S,v 1.3 2014/05/22 15:01:56 uebayasi Exp $")
#endif

#ifdef WEAK_ALIAS
WEAK_ALIAS(ldiv, _ldiv)
#endif

#ifdef WEAK_ALIAS
ENTRY(_ldiv)
#else
ENTRY(ldiv)
#endif
	movq    %rdi, %rax
	cqto
	idivq   %rsi
	ret
#ifdef WEAK_ALIAS
END(_ldiv)
#else
END(ldiv)
#endif
