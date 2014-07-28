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

	/* load gdt and idt prepared by bsp */
	lgdtl	_C_LABEL(__ap_gdt) - _C_LABEL(trampoline)
	lidtl	_C_LABEL(__ap_idt) - _C_LABEL(trampoline)

	/* switch to protected mode */
	mov	%cr0, %eax
	orb	$1, %al
	mov	%eax, %cr0

	/* set page table feature flags: cr4.PSE on, cr4.PGE off */
	movl	%cr4, %eax
	orl	$I386_CR4_PSE, %eax	/* Turn on PSE */
	andl	$~I386_CR4_PGE, %eax	/* Turn off PGE */
	movl	%eax, %cr4

	/* load boot cr3 and turn PG on so CPU can see all of memory */
	movl	_C_LABEL(__ap_pt) - _C_LABEL(trampoline), %eax
	movl	%eax, %cr3
	movl	%cr0, %ecx
	orl	$I386_CR0_PG, %ecx
	movl	%ecx, %cr0

	/* turn on cr4.PGE after cr0.PG is on */
	movl	%cr4, %eax
	orl	$I386_CR4_PGE, %eax
	movl	%eax, %cr4

	/* jump into regular highly mapped kernel */
	ljmpl	$KERN_CS_SELECTOR, $_C_LABEL(startup_ap_32)

.balign 4
LABEL(__ap_id)
.space 4
LABEL(__ap_pt)
.space 4
LABEL(__ap_gdt)
.space 8
LABEL(__ap_idt)
.space 8
LABEL(__ap_gdt_tab)
.space GDT_SIZE*DESC_SIZE
LABEL(__ap_idt_tab)
.space IDT_SIZE*DESC_SIZE
LABEL(__trampoline_end)
