.sect .text; .sect .rom; .sect .data; .sect .bss
.define .cms

	! #bytes in ecx
	.sect .text
.cms:
	pop     ebx              ! return address
	mov     edx,esp
	push	esi
	push	edi
	mov     esi,edx
	add     edx,ecx
	mov     edi,edx
	add     edx,ecx
	sar     ecx,2
	repe cmps
	je      1f
	inc     ecx
1:
	pop	edi
	pop	esi
	mov     esp,edx
	jmp     ebx
