#include <machine/asm.h>
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

	ljmp	$CS_SELECTOR, $_C_LABEL(startup_ap_32)

.balign 4
LABEL(__ap_id)
.space 4
LABEL(__ap_gdt)
.space 8
LABEL(__ap_idt)
.space 8
LABEL(__trampoline_end)
