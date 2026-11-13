#include <machine/asm.h>
#include <machine/vm.h>
#include "archconst.h"

.balign 4096
.text
.code16
ENTRY(trampoline)
	cli

	/* %cs has some value and we must use the same for data */
	mov	%cs, %ax
	mov	%ax, %ds

	/*
	 * Compute the physical address of __ap_startup_64 and store it in
	 * __ap_jmpvec.  In real mode CS * 16 == trampoline_base, so:
	 *   phys(__ap_startup_64) = CS*16 + (__ap_startup_64 - trampoline)
	 * The difference is a link-time constant; %eax is zeroed first because
	 * movw only writes the low 16 bits.
	 */
	xorl	%eax, %eax
	movw	%cs, %ax
	shll	$4, %eax
	addl	$(_C_LABEL(__ap_startup_64) - _C_LABEL(trampoline)), %eax
	movl	%eax, (_C_LABEL(__ap_jmpvec) - _C_LABEL(trampoline))

	/* load gdt and idt prepared by bsp */
	lgdtl	_C_LABEL(__ap_gdt) - _C_LABEL(trampoline)
	lidtl	_C_LABEL(__ap_idt) - _C_LABEL(trampoline)

	/* switch to protected mode */
	mov	%cr0, %eax
	orb	$1, %al
	mov	%eax, %cr0

	/* enable PAE — required for long mode entry */
	movl	%cr4, %eax
	orl	$I386_CR4_PAE, %eax
	movl	%eax, %cr4

	/* load PML4 physical address into CR3 */
	movl	_C_LABEL(__ap_pt) - _C_LABEL(trampoline), %eax
	movl	%eax, %cr3

	/* set EFER.LME to arm long mode */
	movl	$AMD_MSR_EFER, %ecx
	rdmsr
	orl	$AMD_EFER_LME, %eax
	wrmsr

	/* enable paging — activates long mode; CPU is now in IA-32e compat mode */
	movl	%cr0, %ecx
	orl	$I386_CR0_PG, %ecx
	movl	%ecx, %cr0

	/*
	 * Far jump through __ap_jmpvec:
	 *   [32-bit phys addr of __ap_startup_64][KERN_CS_SELECTOR]
	 * Loading the 64-bit CS transitions the CPU from compat to 64-bit mode.
	 * DS still carries the real-mode shadow (base = trampoline_base), so the
	 * DS-relative address below correctly reaches __ap_jmpvec.
	 */
	ljmpl	*(_C_LABEL(__ap_jmpvec) - _C_LABEL(trampoline))

.balign 4
LABEL(__ap_id)
.space 4
LABEL(__ap_pt)
.space 4
LABEL(__ap_gdt)
.space 10			/* struct desctableptr_s: u16 limit + u64 base */
LABEL(__ap_idt)
.space 10			/* struct desctableptr_s: u16 limit + u64 base */
LABEL(__ap_jmpvec)		/* 6-byte far pointer for long-mode entry */
.long	0			/* 32-bit offset: phys addr of __ap_startup_64 (runtime) */
.word	KERN_CS_SELECTOR	/* 16-bit selector: 64-bit kernel code segment */
LABEL(__ap_gdt_tab)
.space GDT_SIZE*DESC_SIZE
LABEL(__ap_idt_tab)
.space IDT_SIZE*GATE_DESC_SIZE	/* gate descriptors are 16 bytes in long mode */

/*
 * 64-bit stub: executed immediately after the far jump into long mode.
 * Runs at its identity-mapped physical address.  Use movabs to load the
 * full 64-bit kernel VA of startup_ap_32 and jump there.
 */
.balign 4
.code64
LABEL(__ap_startup_64)
	movabs	$_C_LABEL(startup_ap_32), %rax
	jmpq	*%rax

LABEL(__trampoline_end)
