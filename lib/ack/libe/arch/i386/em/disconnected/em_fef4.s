.sect .text; .sect .rom; .sect .data; .sect .bss
.define .fef4

	.sect .text
.fef4:
				! this could be simpler, if only the
				! fxtract instruction was emulated properly
	mov	bx,sp
	mov	ax,8(bx)
	and	ax,0x7f800000
	je	1f		! zero exponent
	shr	ax,23
	sub	ax,126
	mov	cx,ax		! exponent in cx
	mov	ax,8(bx)
	and	ax,0x807fffff
	or	ax,0x3f000000	! load -1 exponent
	mov	bx,4(bx)
	mov	4(bx),ax
	mov	(bx),cx
	ret
1:				! we get here on zero exp
	mov	ax,8(bx)
	and	ax,0x007fffff
	jne	1f		! zero result
	mov	bx,4(bx)
	mov	(bx),ax
	mov	4(bx),ax
	ret
1:				! otherwise unnormalized number
	mov	cx,8(bx)
	and	cx,0x807fffff
	mov	dx,cx
	and	cx,0x80000000
	mov	ax,-125
2:
	test	dx,0x800000
	jne	1f
	dec	ax
	shl	dx,1
	or	dx,cx
	jmp	2b
1:
	mov	bx,4(bx)
	mov	(bx),ax
	and	dx,0x807fffff
	or	dx,0x3f000000	! load -1 exponent
	mov	4(bx),dx
	ret
