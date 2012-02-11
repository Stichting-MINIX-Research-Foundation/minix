/*	$NetBSD: fpgetround.S,v 1.5 2002/01/13 21:45:43 thorpej Exp $	*/

/*
 * Written by J.T. Conklin, Apr 4, 1995
 * Public domain.
 */

#include <machine/asm.h>

#ifdef WEAK_ALIAS
WEAK_ALIAS(fpgetround, _fpgetround)
ENTRY(_fpgetround)
#else
ENTRY(fpgetround)
#endif
	subl $4,%esp
	fnstcw (%esp)
	movl (%esp),%eax
	rorl $10,%eax
	andl $3,%eax
	addl $4,%esp
	ret
