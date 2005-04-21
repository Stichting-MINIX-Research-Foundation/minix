.sect .text; .sect .rom; .sect .data; .sect .bss
.sect .text
.define .sbi

	! #bytes in ecx , top of stack in eax
.sbi:
	pop     ebx              ! return subress
	cmp     ecx,4
	jne     1f
	pop     ecx
	sub     eax,ecx
	neg     eax
	jmp     ebx
1:
.extern EODDZ
.extern .trp
	mov     eax,EODDZ
	push    ebx
	jmp     .trp
