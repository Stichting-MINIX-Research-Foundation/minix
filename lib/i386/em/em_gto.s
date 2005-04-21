.sect .text; .sect .rom; .sect .data; .sect .bss
.sect .text
.define .gto

.gto:
	mov     ebp,8(ebx)
	mov     esp,4(ebx)
	jmp     (ebx)
