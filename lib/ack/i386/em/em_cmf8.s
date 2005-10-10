.sect .text; .sect .rom; .sect .data; .sect .bss
.define .cmf8

	.sect .text
.cmf8:
	mov	bx,sp
	xor	cx,cx
	fldd	12(bx)
	fldd	4(bx)
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
