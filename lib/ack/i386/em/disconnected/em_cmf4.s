.sect .text; .sect .rom; .sect .data; .sect .bss
.define .cmf4

	.sect .text
.cmf4:
	mov	bx,sp
	xor	cx,cx
	flds	8(bx)
	flds	4(bx)
	fcompp			! compare and pop operands
	fstsw	ax
	wait
	sahf
	je	1f
	jb	2f
	dec	cx
	jmp	1f
2:
	inc	cx
1:
	mov	ax,cx
	ret
