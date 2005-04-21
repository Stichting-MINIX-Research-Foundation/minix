.sect .text; .sect .rom; .sect .data; .sect .bss
.define .fef8

	.sect .text
.fef8:
				! this could be simpler, if only the
				! fxtract instruction was emulated properly
	mov	bx,sp
	mov	ax,12(bx)
	and	ax,0x7ff00000
	je	1f		! zero exponent
	shr	ax,20
	sub	ax,1022
	mov	cx,ax		! exponent in cx
	mov	ax,12(bx)
	and	ax,0x800fffff
	or	ax,0x3fe00000	! load -1 exponent
	mov	dx,8(bx)
	mov	bx,4(bx)
	mov	4(bx),dx
	mov	8(bx),ax
	mov	(bx),cx
	ret
1:				! we get here on zero exp
	mov	ax,12(bx)
	and	ax,0xfffff
	or	ax,8(bx)
	jne	1f		! zero result
	mov	bx,4(bx)
	mov	(bx),ax
	mov	4(bx),ax
	mov	8(bx),ax
	ret
1:				! otherwise unnormalized number
	mov	cx,12(bx)
	and	cx,0x800fffff
	mov	dx,cx
	and	cx,0x80000000
	mov	ax,-1021
2:
	test	dx,0x100000
	jne	1f
	dec	ax
	shl	8(bx),1
	rcl	dx,1
	or	dx,cx
	jmp	2b
1:
	and	dx,0x800fffff
	or	dx,0x3fe00000	! load -1 exponent
	mov	cx,8(bx)
	mov	bx,4(bx)
	mov	(bx),ax
	mov	8(bx),dx
	mov	4(bx),cx
	ret
