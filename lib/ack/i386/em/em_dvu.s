.sect .text; .sect .rom; .sect .data; .sect .bss
.define .dvu

	! #bytes in eax
	.sect .text
.dvu:
	pop     ebx              ! return address
	cmp     eax,4
	jne     1f
	pop     eax
	xor     edx,edx
	pop     ecx
	div     ecx
	push    eax
	jmp     ebx
1:
.extern EODDZ
.extern .trp
	mov     eax,EODDZ
	push    ebx
	jmp     .trp
