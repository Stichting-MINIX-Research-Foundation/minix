! _cpuid() - interface to cpuid instruction

.sect .text; .sect .rom; .sect .data; .sect .bss
.sect .text

! void _cpuid(u32_t eax, u32_t *eax, u32_t *ebx, u32_t *ecx, u32_t *edx);

.define	__cpuid

__cpuid:
	push	ebp

	mov	ebp, esp

	! save work registers
	push	eax
	push	ebx
	push	ecx
	push	edx

	! set eax parameter to cpuid and execute cpuid
	mov	eax,  24(esp)
	.data1	0x0F, 0xA2	! CPUID

	! store results in pointer arguments
	mov	ebp, 28(esp)
	mov	(ebp), eax
	mov	ebp, 32(esp)
	mov	(ebp), ebx
	mov	ebp, 36(esp)
	mov	(ebp), ecx
	mov	ebp, 40(esp)
	mov	(ebp), edx

	! restore registers
	pop	edx
	pop	ecx
	pop	ebx
	pop	eax

	pop	ebp

	ret
