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

        movq    %rdx, %rax
	movq    %rsi, %rbx
	jmp    *%rdi
multiboot64_end:
