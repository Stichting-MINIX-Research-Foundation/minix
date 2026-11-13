/*	$NetBSD: multiboot64.S,v 1.1.2.2 2019/09/17 19:32:00 martin Exp $ */
#include <machine/asm.h>
#include <machine/specialreg.h>

#define CODE_SEGMENT    0x08
#define DATA_SEGMENT    0x10

	.align	16
	.globl _C_LABEL(multiboot64)
_C_LABEL(multiboot64):
	.quad 0

	.globl _C_LABEL(multiboot64_size)
_C_LABEL(multiboot64_size):
	.long multiboot64_end - _C_LABEL(multiboot64_start)

	.text
	.p2align 4,,15

/*
 * multiboot64(entry %rdi, multiboot2_info %rsi, magic %rdx);
 */
ENTRY(multiboot64_start)
start:
	/*
	 * Enter the Multiboot2 kernel in the required x86-64 state:
	 * interrupts disabled and direction flag clear.  EFI firmware may
	 * leave IF set; the kernel has not installed its own IDT yet.
	 */
	cli
	cld

	/* Multiboot2 passes its magic and information pointer in EAX/EBX. */
	movl	%edx, %eax
	movl	%esi, %ebx
	jmp	*%rdi
multiboot64_end:
