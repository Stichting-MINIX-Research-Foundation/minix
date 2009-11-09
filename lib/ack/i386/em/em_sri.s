.sect .text; .sect .rom; .sect .data; .sect .bss
.sect .text
.define .sri

	! #bytes in eax
.sri:
	pop     edx              ! return address
	cmp     eax,4
	jne     1f
	pop     eax
	pop     ecx
	sar     eax,cl
	push    eax
	jmp     edx
1:
.extern EODDZ
.extern .trp
	mov     eax,EODDZ
	push    edx
	jmp     .trp
