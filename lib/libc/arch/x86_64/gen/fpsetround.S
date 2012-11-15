/*	$NetBSD: fpsetround.S,v 1.4 2011/09/30 17:42:34 christos Exp $	*/

/*
 * Written by Frank van der Linden at Wasabi Systems for NetBSD.
 * Public domain.
 */

#include <machine/asm.h>

/*
 * XXX set both the x87 control word and the SSE mxcsr register.
 * Applications should only set exception and round flags
 * via the fp*() interface, otherwise the status words
 * will get our of sync.
 */ 


#ifdef WEAK_ALIAS
WEAK_ALIAS(fpsetround, _fpsetround)
ENTRY(_fpsetround)
#else
ENTRY(fpsetround)
#endif

	fnstcw	-4(%rsp)
	movl	-4(%rsp), %edx
	movl	%edx, %eax
	andl	$0x00000c00, %eax	
	andl	$0xfffff3ff, %edx
	orl	%edi, %edx
	movl	%edx, -4(%rsp)
	fldcw	-4(%rsp)

	stmxcsr	-4(%rsp)
	movl	-4(%rsp), %edx
	andl	$0xffff9fff, %edx
	sall	$3, %edi
	orl	%edi,%edx
	movl	%edx,-4(%rsp)
	ldmxcsr	-4(%rsp)

	ret
