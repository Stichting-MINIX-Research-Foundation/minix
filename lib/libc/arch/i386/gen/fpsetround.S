/*	$NetBSD: fpsetround.S,v 1.5 2011/09/30 23:45:41 christos Exp $	*/

/*
 * Written by Frank van der Linden at Wasabi Systems for NetBSD.
 * Public domain.
 */

#include <machine/asm.h>

/*
 * XXX set both the x87 control word
 * Applications should only set exception and round flags
 */ 


#ifdef WEAK_ALIAS
WEAK_ALIAS(fpsetround, _fpsetround)
ENTRY(_fpsetround)
#else
ENTRY(fpsetround)
#endif

	fnstcw	-4(%esp)
	movl	-4(%esp), %edx
	movl	%edx, %eax
	andl	$0x00000c00, %eax	
	andl	$0xfffff3ff, %edx
	movl	4(%esp), %ecx
	orl	%ecx, %edx
	movl	%edx, -4(%esp)
	fldcw	-4(%esp)

	ret
