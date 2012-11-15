/*	$NetBSD: fpgetprec.S,v 1.1 2011/03/26 19:51:42 christos Exp $	*/

/*
 * Written by J.T. Conklin, Apr 4, 1995
 * Public domain.
 */

#include <machine/asm.h>

/*
 * XXX store only x87 state. If an application only uses the fp*
 * interface, this should be in sync with the SSE mxcsr register.
 */

#ifdef WEAK_ALIAS
WEAK_ALIAS(fpgetprec, _fpgetprec)
ENTRY(_fpgetprec)
#else
ENTRY(fpgetprec)
#endif
	fnstcw -4(%rsp)
	movl -4(%rsp),%eax
	rorl $8,%eax
	andl $3,%eax
	ret
