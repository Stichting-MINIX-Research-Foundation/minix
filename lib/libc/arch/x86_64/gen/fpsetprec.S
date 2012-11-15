/*	$NetBSD: fpsetprec.S,v 1.1 2011/03/26 19:51:42 christos Exp $	*/

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
WEAK_ALIAS(fpsetprec, _fpsetprec)
ENTRY(_fpsetprec)
#else
ENTRY(fpsetprec)
#endif
	fnstcw	-4(%rsp)

	andl	$3,%edi

	movl	-4(%rsp),%edx
	rorl	$8,%edx
	movl	%edx,%eax
	andl	$3,%eax

	andl	$~3,%edx
	orl	%edi,%edx
	roll	$8,%edx
	movl	%edx,-4(%rsp)

	fldcw	-4(%rsp)
	ret
