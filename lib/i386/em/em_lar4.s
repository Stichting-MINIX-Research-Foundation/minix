.sect .text; .sect .rom; .sect .data; .sect .bss
.sect .text
.define .lar4

.lar4:
				! ebx, descriptor address
				! eax, index
	sub     eax,(ebx)
	mov     ecx,8(ebx)
	imul    ecx
	pop	ebx
	pop	edx		! base address
	add     edx,eax
	sar     ecx,1
	jnb     1f
	xor     eax,eax
	movb	al,(edx)
	push    eax
	jmp     ebx
1:
	sar     ecx,1
	jnb     1f
	xor     eax,eax
	o16 mov	ax,(edx)
	push    eax
	jmp     ebx
1:
	xchg	edx,esi		! saved esi
	mov	eax,ecx
	sal	eax,2
	sub     esp,eax
	mov	eax,edi		! save edi
	mov     edi,esp
	rep movs
	mov	edi,eax
	mov	esi,edx
	jmp     ebx
