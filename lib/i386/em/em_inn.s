.sect .text; .sect .rom; .sect .data; .sect .bss
.sect .text
.define .inn

	! #bytes in ecx
	! bit # in eax
.inn:
	xor     edx,edx
	mov     ebx,8
	div     ebx
	mov     ebx,esp
	add	ebx,4
	add     ebx,eax
	cmp     eax,ecx
	jae     1f
	movb	al,(ebx)
	mov	ebx,edx
	testb   al,bits(ebx)
	jz      1f
	mov	eax,1
	jmp	2f
1:
	xor	eax,eax
2:
	pop	ebx
	add     esp,ecx
	! eax is result
	jmp     ebx

	.sect .rom
bits:
	.data1 1,2,4,8,16,32,64,128
