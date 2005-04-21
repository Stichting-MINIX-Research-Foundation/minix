.sect .text; .sect .rom; .sect .data; .sect .bss
.define .csa4

.sect .text
.csa4:
				! ebx, descriptor address
				! eax, index
	mov     edx,(ebx)         ! default
	sub     eax,4(ebx)
	cmp     eax,8(ebx)
	ja      1f
	sal     eax,2
	add	ebx,eax
	mov     ebx,12(ebx)
	test    ebx,ebx
	jnz     2f
1:
	mov     ebx,edx
	test    ebx,ebx
	jnz     2f
.extern ECASE
.extern .fat
	mov     eax,ECASE
	push    eax
	jmp     .fat
2:
	jmp     ebx
