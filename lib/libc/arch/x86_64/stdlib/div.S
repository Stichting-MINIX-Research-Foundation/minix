/*	$NetBSD: div.S,v 1.1 2001/06/19 00:25:04 fvdl Exp $	*/
/*
 * Written by Frank van der Linden (fvdl@wasabisystems.com)
 * Public domain.
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS)
RCSID("$NetBSD: div.S,v 1.1 2001/06/19 00:25:04 fvdl Exp $")
#endif

ENTRY(div)
	movl	%edi, %eax
	cltd
	idivl	%esi
	salq	$32, %rdx
	orq	%rdx,%rax
	ret
