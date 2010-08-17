/*	cmp64*() - 64 bit compare			Author: Kees J. Bot */
/*								7 Dec 1995 */
#include <machine/asm.h>

ENTRY(cmp64)
/* int cmp64(u64_t i, u64_t j); */
	movl	%esp, %ecx
0:
	xorl	%eax, %eax
	movl	4(%ecx), %edx
	subl	12(%ecx), %edx
	movl	8(%ecx), %edx
	sbbl	16(%ecx), %edx
	sbbl	%eax, %eax	/* eax = - (i < j) */
	movl	12(%ecx), %edx
	subl	4(%ecx), %edx
	movl	16(%ecx), %edx
	sbbl	8(%ecx), %edx
	adcl	$0, %eax	/* eax = (i > j) - (i < j) */
	ret

ENTRY(cmp64u)
/* int cmp64u(u64_t i, unsigned j); */
ENTRY(cmp64ul)
/* int cmp64ul(u64_t i, unsigned long j); */
	movl	%esp, %ecx
	push	16(%ecx)
	movl	$0, 16(%ecx)
	call	0b
	pop	16(%ecx)
	ret

/* */
/* $PchId: cmp64.ack.s,v 1.2 1996/04/11 18:59:57 philip Exp $ */
