.sect .text; .sect .rom; .sect .data; .sect .bss
.define	.dup

	! #bytes in ecx
	.sect .text
.dup:
	pop	ebx		! return address
	mov	eax,esi
	mov	edx,edi
	mov	esi,esp
	sub	esp,ecx
	mov	edi,esp
	sar	ecx,2
	rep movs
	mov	esi,eax
	mov	edi,edx
	jmp	ebx
