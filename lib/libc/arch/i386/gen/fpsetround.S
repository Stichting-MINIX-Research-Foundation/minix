/*	$NetBSD: fpsetround.S,v 1.6 2014/05/23 02:34:19 uebayasi Exp $	*/

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
#ifdef WEAK_ALIAS
END(_fpsetround)
#else
END(fpsetround)
#endif
