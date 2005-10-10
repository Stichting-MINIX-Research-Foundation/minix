#
.sect .text; .sect .rom; .sect .data; .sect .bss
.extern _frexp
.sect .text
_frexp:
#if __i386
	push	ebp
	mov	ebp, esp
	push	12(ebp)
	push	8(ebp)
	mov	eax, esp
	add	eax, -4
	push	eax
	call	.fef8
	mov	eax, 16(ebp)
	pop	(eax)
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
	mov	ax, sp
	add	ax, #-2
	push	ax
	call	.fef8
	mov	bx, 12(bp)
	pop	(bx)
	call	.ret8
	jmp	.cret
#endif
