/*	$NetBSD: fpgetsticky.S,v 1.4 2014/05/22 15:01:56 uebayasi Exp $	*/

/*
 * Written by Frank van der Linden at Wasabi Systems for NetBSD.
 * Public domain.
 *
 */

#include <machine/asm.h>

/*
 * XXX read both the x87 and SSE status words here, and OR
 * them to get a complete picture of exceptions.
 */

#ifdef WEAK_ALIAS
WEAK_ALIAS(fpgetsticky, _fpgetsticky)
ENTRY(_fpgetsticky)
#else
ENTRY(fpgetsticky)
#endif
	fnstsw	-4(%rsp)
	stmxcsr	-8(%rsp)
	movl	-4(%rsp),%eax
	orl	-8(%rsp),%eax
	andl	$63,%eax
	ret
#ifdef WEAK_ALIAS
END(_fpgetsticky)
#else
END(fpgetsticky)
#endif
