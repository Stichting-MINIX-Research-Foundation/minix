.sect .text; .sect .rom; .sect .data; .sect .bss
.sect .text
.define	printc,printd,prints

	! argument in eax
	! uses ebx
prints:
	xchg	eax,ebx
1:
	movb	al,(ebx)
	inc	ebx
	testb	al,al
	jz	2f
	call	printc
	jmp	1b
2:
	ret

	! argument in eax
	! uses ecx and edx
printd:
	xor	edx,edx
	mov	ecx,10
	div	ecx
	test	eax,eax
	jz	1f
	push	edx
	call	printd
	pop	edx
1:
	xchg	eax,edx
	addb	al,'0'

	! argument in eax
printc:
	push	eax
	mov	ebx,esp
	mov	eax,1
	push	eax
	push	ebx
	push	eax
	call	__write
	pop	ebx
	pop	ebx
	pop	ebx
	pop	ebx
	ret
