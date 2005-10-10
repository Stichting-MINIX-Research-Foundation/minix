.sect .text; .sect .rom; .sect .data; .sect .bss
.sect .text
.define .ngi

	! #bytes in eax
.ngi:
	pop     ebx              ! return address
	cmp     eax,4
	jne     1f
	pop     ecx
	neg     ecx
	push    ecx
	jmp     ebx
1:
.extern EODDZ
.extern .trp
	mov     eax,EODDZ
	push    ebx
	jmp     .trp
