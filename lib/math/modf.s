#
.sect .text; .sect .rom; .sect .data; .sect .bss
.extern _modf
.sect .text
_modf:
#if __i386
	push	ebp
	mov	ebp, esp
	push	12(ebp)
	push	8(ebp)
	push	1
	push	4
	call	.cif8
	mov	eax, esp
	push	eax
	call	.fif8
	pop	ecx
	mov	edx, 16(ebp)
	pop	ecx
	pop	ebx
	mov	0(edx), ecx
	mov	4(edx), ebx
	pop	eax
	pop	edx
	leave
	ret
#else /* i86 */
	push	bp
	mov	bp, sp
	lea	bx, 4(bp)
	mov	cx, #8
	call	.loi
	mov	dx, #1
	push	dx
	push	dx
	push	dx
	mov	ax, #2
	push	ax
	call	.cif8
	mov	ax, sp
	push	ax
	call	.fif8
	pop	bx
	mov	bx, 12(bp)
	mov	cx, #8
	call	.sti
	call	.ret8
	jmp	.cret
#endif
