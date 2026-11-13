/*	$NetBSD: efi_stub.S,v 1.1.1.1 2014/04/01 16:16:07 jakllsch Exp $	*/

/*
 * Function calling ABI conversion from Linux to EFI for x86_64
 *
 * Copyright (C) 2007 Intel Corp
 *	Bibo Mao <bibo.mao@intel.com>
 *	Huang Ying <ying.huang@intel.com>
 * Copyright (C) 2012 Felipe Contreras <felipe.contreras@gmail.com>
 */

#if !defined(HAVE_USE_MS_ABI)
/*
 * EFI calling conventions are documented at:
 *   http://msdn.microsoft.com/en-us/library/ms235286%28v=vs.80%29.aspx
 * ELF calling conventions are documented at:
 *   http://www.x86-64.org/documentation/abi.pdf
 *
 * Basically here are the conversion rules:
 * a) our function pointer is in %rdi
 * b) rsi through r8 (elf) aka rcx through r9 (ms) require stack space
 *    on the MS side even though it's not getting used at all.
 * c) 8(%rsp) is always aligned to 16 in ELF, so %rsp is shifted 8 bytes extra
 * d) arguments are as follows: (elf -> ms)
 *   1) rdi -> rcx (32 saved)
 *   2) rsi -> rdx (32 saved)
 *   3) rdx -> r8 (32 saved)
 *   4) rcx -> r9 (32 saved)
 *   5) r8 -> 32(%rsp) (32 saved)
 *   6) r9 -> 40(%rsp) (48 saved)
 *   7) 8(%rsp) -> 48(%rsp) (48 saved)
 *   8) 16(%rsp) -> 56(%rsp) (64 saved)
 *   9) 24(%rsp) -> 64(%rsp) (64 saved)
 *  10) 32(%rsp) -> 72(%rsp) (80 saved)
 * e) because the first argument we recieve in a thunker is actually the
 *    function to be called, arguments are offset as such:
 *   0) rdi -> caller
 *   1) rsi -> rcx (32 saved)
 *   2) rdx -> rdx (32 saved)
 *   3) rcx -> r8 (32 saved)
 *   4) r8 -> r9 (32 saved)
 *   5) r9 -> 32(%rsp) (32 saved)
 *   6) 8(%rsp) -> 40(%rsp) (48 saved)
 *   7) 16(%rsp) -> 48(%rsp) (48 saved)
 *   8) 24(%rsp) -> 56(%rsp) (64 saved)
 *   9) 32(%rsp) -> 64(%rsp) (64 saved)
 *  10) 40(%rsp) -> 72(%rsp) (80 saved)
 * f) arguments need to be moved in opposite order to avoid clobbering
 */

#define ENTRY(name)	\
	.globl name;	\
	name:

ENTRY(efi_call0)
	subq $40, %rsp
	call *%rdi
	addq $40, %rsp
	ret

ENTRY(efi_call1)
	subq $40, %rsp
	mov  %rsi, %rcx
	call *%rdi
	addq $40, %rsp
	ret

ENTRY(efi_call2)
	subq $40, %rsp
	/* mov %rdx, %rdx */
	mov  %rsi, %rcx
	call *%rdi
	addq $40, %rsp
	ret

ENTRY(efi_call3)
	subq $40, %rsp
	mov  %rcx, %r8
	/* mov %rdx, %rdx */
	mov  %rsi, %rcx
	call *%rdi
	addq $40, %rsp
	ret

ENTRY(efi_call4)
	subq $40, %rsp
	mov %r8, %r9
	mov %rcx, %r8
	/* mov %rdx, %rdx */
	mov %rsi, %rcx
	call *%rdi
	addq $40, %rsp
	ret

ENTRY(efi_call5)
	subq $40, %rsp
	mov %r9, 32(%rsp)
	mov %r8, %r9
	mov %rcx, %r8
	/* mov %rdx, %rdx */
	mov %rsi, %rcx
	call *%rdi
	addq $40, %rsp
	ret

ENTRY(efi_call6)
	subq $56, %rsp
	mov 56+8(%rsp), %rax
	mov %rax, 40(%rsp)
	mov %r9, 32(%rsp)
	mov %r8, %r9
	mov %rcx, %r8
	/* mov %rdx, %rdx */
	mov %rsi, %rcx
	call *%rdi
	addq $56, %rsp
	ret

ENTRY(efi_call7)
	subq $56, %rsp
	mov 56+16(%rsp), %rax
	mov %rax, 48(%rsp)
	mov 56+8(%rsp), %rax
	mov %rax, 40(%rsp)
	mov %r9, 32(%rsp)
	mov %r8, %r9
	mov %rcx, %r8
	/* mov %rdx, %rdx */
	mov %rsi, %rcx
	call *%rdi
	addq $56, %rsp
	ret

ENTRY(efi_call8)
	subq $72, %rsp
	mov 72+24(%rsp), %rax
	mov %rax, 56(%rsp)
	mov 72+16(%rsp), %rax
	mov %rax, 48(%rsp)
	mov 72+8(%rsp), %rax
	mov %rax, 40(%rsp)
	mov %r9, 32(%rsp)
	mov %r8, %r9
	mov %rcx, %r8
	/* mov %rdx, %rdx */
	mov %rsi, %rcx
	call *%rdi
	addq $72, %rsp
	ret

ENTRY(efi_call9)
	subq $72, %rsp
	mov 72+32(%rsp), %rax
	mov %rax, 64(%rsp)
	mov 72+24(%rsp), %rax
	mov %rax, 56(%rsp)
	mov 72+16(%rsp), %rax
	mov %rax, 48(%rsp)
	mov 72+8(%rsp), %rax
	mov %rax, 40(%rsp)
	mov %r9, 32(%rsp)
	mov %r8, %r9
	mov %rcx, %r8
	/* mov %rdx, %rdx */
	mov %rsi, %rcx
	call *%rdi
	addq $72, %rsp
	ret

ENTRY(efi_call10)
	subq $88, %rsp
	mov 88+40(%rsp), %rax
	mov %rax, 72(%rsp)
	mov 88+32(%rsp), %rax
	mov %rax, 64(%rsp)
	mov 88+24(%rsp), %rax
	mov %rax, 56(%rsp)
	mov 88+16(%rsp), %rax
	mov %rax, 48(%rsp)
	mov 88+8(%rsp), %rax
	mov %rax, 40(%rsp)
	mov %r9, 32(%rsp)
	mov %r8, %r9
	mov %rcx, %r8
	/* mov %rdx, %rdx */
	mov %rsi, %rcx
	call *%rdi
	addq $88, %rsp
	ret

#endif
