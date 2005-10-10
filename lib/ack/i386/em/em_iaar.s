.sect .text; .sect .rom; .sect .data; .sect .bss
.sect .text
.define .iaar

.iaar:
	pop     ecx
	pop     edx
	cmp     edx,4
.extern .unknown
	jne     .unknown
	pop     ebx     ! descriptor address
	pop     eax     ! index
	sub     eax,(ebx)
	mul     8(ebx)
	pop	ebx	! array base
	add     ebx,eax
	push	ecx
	ret
