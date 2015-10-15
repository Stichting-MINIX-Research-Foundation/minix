/*	$NetBSD: alloca.S,v 1.2 2014/05/22 15:01:56 uebayasi Exp $	*/

#include <machine/asm.h>

#if defined(LIBC_SCCS)
	RCSID("$NetBSD: alloca.S,v 1.2 2014/05/22 15:01:56 uebayasi Exp $")
#endif

ENTRY(alloca)
	popq	%rdx
	movq	%rsp,%rcx
	addq	$15,%rdi	/*  round up to 16 bytes */
	andq	$~15,%rdi
	subq	%rdi,%rsp
	movq	%rsp,%rax
	jmp	*%rdx
END(alloca)
