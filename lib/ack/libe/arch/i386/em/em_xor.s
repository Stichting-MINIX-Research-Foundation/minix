.sect .text; .sect .rom; .sect .data; .sect .bss
.sect .text
.define	.xor

	! #bytes in ecx
.xor:
	pop	ebx		! return address
	mov	edx,edi
	mov	edi,esp
	add	edi,ecx
	sar	ecx,2
1:
	pop	eax
	xor	eax,(edi)
	stos
	loop	1b
	mov	edi,edx
	jmp	ebx
