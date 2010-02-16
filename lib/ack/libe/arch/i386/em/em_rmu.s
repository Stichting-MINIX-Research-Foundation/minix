.sect .text; .sect .rom; .sect .data; .sect .bss
.sect .text
.define .rmu

	! #bytes in eax
.rmu:
	pop     ebx              ! return address
	cmp     eax,4
	jne     1f
	pop     eax
	xor     edx,edx
	pop     ecx
	idiv    ecx
	push    edx
	jmp     ebx
1:
.extern EODDZ
.extern .trp
	mov     eax,EODDZ
	push    ebx
	jmp     .trp
