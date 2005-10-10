.sect .text; .sect .rom; .sect .data; .sect .bss
.define .adi

	! #bytes in ecx , top of stack in eax
	.sect .text
.adi:
	pop     ebx              ! return address
	cmp     ecx,4
	jne     9f
	pop     ecx
	add     eax,ecx
	jmp     ebx
9:
.extern	EODDZ
.extern .trp
	mov     eax,EODDZ
	push	ebx
	jmp     .trp
