/*	$NetBSD: div.S,v 1.2 2014/05/22 15:01:56 uebayasi Exp $	*/
/*
 * Written by Frank van der Linden (fvdl@wasabisystems.com)
 * Public domain.
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS)
RCSID("$NetBSD: div.S,v 1.2 2014/05/22 15:01:56 uebayasi Exp $")
#endif

ENTRY(div)
	movl	%edi, %eax
	cltd
	idivl	%esi
	salq	$32, %rdx
	orq	%rdx,%rax
	ret
END(div)
