/*	$NetBSD: fpsetmask.S,v 1.6 2014/05/22 15:01:56 uebayasi Exp $	*/

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
WEAK_ALIAS(fpsetmask, _fpsetmask)
ENTRY(_fpsetmask)
#else
ENTRY(fpsetmask)
#endif
	notl	%edi
	andl	$0x0000003f,%edi

	fnstcw	-4(%rsp)
	movl	-4(%rsp), %edx
	movl	%edx, %eax
	andl	$0xffffffc0, %edx
	orl	%edi, %edx
	movl	%edx,-4(%rsp)
	fldcw	-4(%rsp)

	stmxcsr	-4(%rsp)
	movl	-4(%rsp), %edx
	andl	$0xffffe07f, %edx
	sall	$7, %edi
	orl	%edi, %edx
	movl	%edx,-4(%rsp)
	ldmxcsr	-4(%rsp)

	notl	%eax
	andl	$0x0000003f, %eax
	ret
#ifdef WEAK_ALIAS
END(_fpsetmask)
#else
END(fpsetmask)
#endif
