.define	.dup

	! #bytes in cx
	.text
.dup:
	pop	bx		! return address
	mov	ax,si
	mov	dx,di
	mov	si,sp
	sub	sp,cx
	mov	di,sp
	sar	cx,#1
	rep
	mov
	mov	si,ax
	mov	di,dx
	jmp	(bx)
