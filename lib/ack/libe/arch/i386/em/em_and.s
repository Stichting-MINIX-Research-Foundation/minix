.sect .text; .sect .rom; .sect .data; .sect .bss
.define	.and

	! #bytes in ecx
	! save edi; it might be a register variable

	.sect .text
.and:
	pop	ebx		! return address
	mov	edx,edi
	mov	edi,esp
	add	edi,ecx
	sar	ecx,2
1:
	pop	eax
	and	eax,(edi)
	stos
	loop	1b
	mov	edi,edx
	jmp	ebx
