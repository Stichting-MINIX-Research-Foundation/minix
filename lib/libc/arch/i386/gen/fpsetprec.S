/*	$NetBSD: fpsetprec.S,v 1.2 2014/05/23 02:34:19 uebayasi Exp $	*/

/*
 * Written by Charles M. Hannum, Apr 9, 1995
 * Public domain.
 */

#include <machine/asm.h>

#ifdef WEAK_ALIAS
WEAK_ALIAS(fpsetprec, _fpsetprec)
ENTRY(_fpsetprec)
#else
ENTRY(fpsetprec)
#endif
	subl $4,%esp

	fnstcw (%esp)
	movl (%esp),%eax

	rorl $8,%eax
	movl %eax,%edx
	andl $3,%eax

	subl %eax,%edx
	movl 8(%esp),%ecx
	andl $3,%ecx
	orl %ecx,%edx
	roll $8,%edx
	movl %edx,(%esp)
	fldcw (%esp)

	addl $4,%esp
	ret
#ifdef WEAK_ALIAS
END(_fpsetprec)
#else
END(fpsetprec)
#endif
