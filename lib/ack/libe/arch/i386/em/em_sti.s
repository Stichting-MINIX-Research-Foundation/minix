.sect .text; .sect .rom; .sect .data; .sect .bss
.sect .text
.define .sti
.define .sts

	! #bytes in ecx
	! address in ebx
	! save edi/esi. they might be register variables
.sts:
	pop	edx
	sar     ecx,1
	jnb     1f
	pop     eax
	movb	(ebx),al
	jmp     edx
1:
	sar     ecx,1
	jnb     1f
	pop     eax
	o16 mov	(ebx),ax
	jmp     edx
1:
	push	edx
	mov	edx,edi
	mov	edi,ebx
	pop	ebx
	jmp	1f
.sti:
	! only called with count >> 4
	mov	edx,edi
	mov	edi,ebx
	pop	ebx
	sar	ecx,2
1:
	mov	eax,esi
	mov     esi,esp
	rep movs
	mov     esp,esi
	mov	edi,edx
	mov	esi,eax
	jmp     ebx
