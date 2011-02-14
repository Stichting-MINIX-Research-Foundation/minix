/*	$NetBSD: fpsetmask.S,v 1.4 2002/01/13 21:45:43 thorpej Exp $	*/

/*
 * Written by Charles M. Hannum, Apr 9, 1995
 * Public domain.
 */

#include <machine/asm.h>

#ifdef WEAK_ALIAS
WEAK_ALIAS(fpsetmask, _fpsetmask)
ENTRY(_fpsetmask)
#else
ENTRY(fpsetmask)
#endif
	subl $4,%esp

	fnstcw (%esp)
	movl (%esp),%eax
	movl %eax,%edx

	notl %eax
	andl $63,%eax

	addl %eax,%edx
	movl 8(%esp),%ecx
	andl $63,%ecx
	subl %ecx,%edx
	movl %edx,(%esp)
	fldcw (%esp)

	addl $4,%esp
	ret
