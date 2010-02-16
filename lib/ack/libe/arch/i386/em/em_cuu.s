.sect .text; .sect .rom; .sect .data; .sect .bss
.define .ciu
.define .cui
.define .cuu

.sect .text
.ciu:
.cui:
.cuu:
	pop     ebx              ! return address
				! pop     ecx, dest. size
				! pop     edx, source size
				! eax is source
	cmp     edx,ecx
	jne     8f
	jmp     ebx
8:
.extern EILLINS
.extern .fat
	mov     eax,EILLINS
	push    eax
	jmp     .fat
