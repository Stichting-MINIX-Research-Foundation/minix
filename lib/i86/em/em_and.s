.define	.and

	! #bytes in cx
	! save di; it might be a register variable

	.text
.and:
	pop	bx		! return address
	mov	dx,di
	mov	di,sp
	add	di,cx
	sar	cx,#1
1:
	pop	ax
	and	ax,(di)
	stos
	loop	1b
	mov	di,dx
	jmp	(bx)
