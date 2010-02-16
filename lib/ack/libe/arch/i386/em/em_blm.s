.sect .text; .sect .rom; .sect .data; .sect .bss
.sect .text
.define .blm

	! ecx: count in words
.blm:
	mov	ebx,esp
	mov	eax,esi
	mov	edx,edi
	mov	edi,4(ebx)
	mov	esi,8(ebx)
	rep	movs
	mov	esi,eax
	mov	edi,edx
	ret	8

