/*	$NetBSD: setjmp.S,v 1.1.1.1 2018/08/16 18:17:47 jmcneill Exp $	*/

	.text
	.globl	setjmp
#ifndef __MINGW32__
	.type setjmp, @function
#else
	.def setjmp; .scl 2; .type 32; .endef
#endif
setjmp:
	pop	%rsi
	movq	%rbx,0x00(%rdi)
	movq	%rsp,0x08(%rdi)
	push	%rsi
	movq	%rbp,0x10(%rdi)
	movq	%r12,0x18(%rdi)
	movq	%r13,0x20(%rdi)
	movq	%r14,0x28(%rdi)
	movq	%r15,0x30(%rdi)
	movq	%rsi,0x38(%rdi)
	xor	%rax,%rax
	ret

	.globl	longjmp
#ifndef __MINGW32__
	.type	longjmp, @function
#else
	.def longjmp; .scl 2; .type 32; .endef
#endif
longjmp:
	movl	%esi, %eax
	movq	0x00(%rdi), %rbx
	movq	0x08(%rdi), %rsp
	movq	0x10(%rdi), %rbp
	movq	0x18(%rdi), %r12
	movq	0x20(%rdi), %r13
	movq	0x28(%rdi), %r14
	movq	0x30(%rdi), %r15
	xor	%rdx,%rdx
	mov	$1,%rcx
	cmp	%rax,%rdx
	cmove	%rcx,%rax
	jmp	*0x38(%rdi)
