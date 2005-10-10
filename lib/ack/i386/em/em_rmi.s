.sect .text; .sect .rom; .sect .data; .sect .bss
.sect .text
.define .rmi

	! #bytes in eax
.rmi:
	pop     ebx              ! return address
	cmp     eax,4
	jne     1f
	pop     eax
	cwd
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
