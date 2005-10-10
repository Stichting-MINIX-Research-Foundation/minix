.sect .text; .sect .rom; .sect .data; .sect .bss
.sect .text
.define .mli

	! #bytes in eax
.mli:
	pop     ebx              ! return address
	cmp     eax,4
	jne     1f
	pop     eax
	pop     ecx
	mul     ecx
	push    eax
	jmp     ebx
1:
.extern EODDZ
.extern .trp
	mov     eax,EODDZ
	push    ebx
	jmp     .trp
