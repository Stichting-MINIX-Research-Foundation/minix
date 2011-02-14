/*	$NetBSD: fpgetsticky.S,v 1.6 2002/01/13 21:45:43 thorpej Exp $	*/

/*
 * Written by J.T. Conklin, Apr 4, 1995
 * Public domain.
 */

#include <machine/asm.h>

#ifdef WEAK_ALIAS
WEAK_ALIAS(fpgetsticky, _fpgetsticky)
ENTRY(_fpgetsticky)
#else
ENTRY(fpgetsticky)
#endif
	subl $4,%esp
	fnstsw (%esp)
	movl (%esp),%eax
	andl $63,%eax
	addl $4,%esp
	ret
