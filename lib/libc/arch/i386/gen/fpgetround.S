/*	$NetBSD: fpgetround.S,v 1.6 2011/09/30 23:42:00 christos Exp $	*/

/*
 * Written by J.T. Conklin, Apr 4, 1995
 * Public domain.
 */

#include <machine/asm.h>

/*
 * XXX load only x87 state.
 */

#ifdef WEAK_ALIAS
WEAK_ALIAS(fpgetround, _fpgetround)
ENTRY(_fpgetround)
#else
ENTRY(fpgetround)
#endif
	fnstcw -4(%esp)
	movl -4(%esp), %eax
	andl $0x00000c00, %eax
	ret
