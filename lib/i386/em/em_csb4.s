.sect .text; .sect .rom; .sect .data; .sect .bss
.define .csb4

.sect .text
.csb4:
				!ebx, descriptor address
				!eax,  index
	mov	edx,(ebx)
	mov	ecx,4(ebx)
1:
	add	ebx,8
	dec     ecx
	jl      4f
	cmp     eax,(ebx)
	jne     1b
	mov	ebx,4(ebx)
2:
	test    ebx,ebx
	jnz     3f
.extern ECASE
.extern .fat
	mov     eax,ECASE
	push    eax
	jmp     .fat
3:
	jmp     ebx
4:
	mov	ebx,edx
	jmp	2b
