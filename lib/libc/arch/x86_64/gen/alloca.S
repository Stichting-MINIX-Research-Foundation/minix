/*	$NetBSD: alloca.S,v 1.1 2001/06/19 00:25:02 fvdl Exp $	*/

#include <machine/asm.h>

#if defined(LIBC_SCCS)
	RCSID("$NetBSD: alloca.S,v 1.1 2001/06/19 00:25:02 fvdl Exp $")
#endif

ENTRY(alloca)
	popq	%rdx
	movq	%rsp,%rcx
	addq	$15,%rdi	/*  round up to 16 bytes */
	andq	$~15,%rdi
	subq	%rdi,%rsp
	movq	%rsp,%rax
	jmp	*%rdx
