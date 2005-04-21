.sect .text; .sect .rom; .sect .data; .sect .bss
.sect .text
.define	.ior

	! #bytes in ecx
.ior:
	pop	ebx		! return address
	mov	edx,edi
	mov	edi,esp
	add	edi,ecx
	sar	ecx,2
1:
	pop	eax
	or	eax,(edi)
	stos
	loop	1b
	mov	edi,edx
	jmp	ebx
