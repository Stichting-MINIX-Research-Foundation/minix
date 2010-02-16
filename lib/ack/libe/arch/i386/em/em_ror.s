.sect .text; .sect .rom; .sect .data; .sect .bss
.sect .text
.define .ror

	! #bytes in eax
.ror:
	pop     edx              ! return address
	cmp     eax,4
	jne     1f
	pop     eax
	pop     ecx
	ror     eax,cl
	push    eax
	jmp     edx
1:
.extern EODDZ
.extern .trp
	mov     eax,EODDZ
	push    edx
	jmp     .trp
