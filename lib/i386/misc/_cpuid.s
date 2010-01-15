! _cpuid() - interface to cpuid instruction

.sect .text; .sect .rom; .sect .data; .sect .bss
.sect .text

! void _cpuid(u32_t *eax, u32_t *ebx, u32_t *ecx, u32_t *edx);

.define	__cpuid

__cpuid:
	! save registers
	push	ebp
	push	ebx

	! set parameters to cpuid and execute cpuid
	mov	ebp,  12(esp)
	mov	eax, (ebp)
	mov	ebp,  16(esp)
	mov	ebx, (ebp)
	mov	ebp,  20(esp)
	mov	ecx, (ebp)
	mov	ebp,  24(esp)
	mov	edx, (ebp)
	.data1	0x0F, 0xA2	! CPUID

	! store results in pointer arguments
	mov	ebp, 12(esp)
	mov	(ebp), eax
	mov	ebp, 16(esp)
	mov	(ebp), ebx
	mov	ebp, 20(esp)
	mov	(ebp), ecx
	mov	ebp, 24(esp)
	mov	(ebp), edx

	! restore registers
	pop	ebx
	pop	ebp

	ret
