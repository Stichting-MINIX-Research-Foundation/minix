.sect .text; .sect .rom; .sect .data; .sect .bss
.sect .text
.define .ilar

.ilar:
	pop     ecx
	pop     edx
.extern .unknown
	cmp     edx,4
	jne     .unknown
	pop     ebx      ! descriptor address
	pop     eax      ! index
	push    ecx
.extern .lar4
	jmp    .lar4
