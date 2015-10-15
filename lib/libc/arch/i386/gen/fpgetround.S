/*	$NetBSD: fpgetround.S,v 1.7 2014/05/23 02:34:19 uebayasi Exp $	*/

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
#ifdef WEAK_ALIAS
END(_fpgetround)
#else
END(fpgetround)
#endif
