
.sect .text; .sect .rom; .sect .data; .sect .bss                             

.define _i386_invlpg

.sect .text

!*===========================================================================*
!*                              i386_invlpg                                  *
!*===========================================================================*
! PUBLIC void i386_invlpg(u32_t addr)
! Tell the processor to invalidate a tlb entry at virtual address addr.
_i386_invlpg:
	push	ebp
	mov	ebp, esp
	push	eax

	mov	eax, 8(ebp)
	invlpg	eax

	pop	eax
	pop	ebp
	ret
