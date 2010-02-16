.sect .text; .sect .rom; .sect .data; .sect .bss
.sect .text
.define .sar4

.sar4:
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
	pop     eax
	movb	(edx),al
	jmp     ebx
1:
	sar     ecx,1
	jnb     1f
	pop     eax
	o16 mov (edx),ax
	jmp     ebx
1:
	xchg	edi,edx		! edi = base address, edx is saved edi
	mov	eax,esi
	mov     esi,esp
	rep movs
	mov     esp,esi
	mov	esi,eax
	mov	edi,edx
	jmp     ebx
