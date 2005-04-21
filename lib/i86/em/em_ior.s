.define	.ior
.text

	! #bytes in cx
.ior:
	pop	bx		! return address
	mov	dx,di
	mov	di,sp
	add	di,cx
	sar	cx,#1
1:
	pop	ax
	or	ax,(di)
	stos
	loop	1b
	mov	di,dx
	jmp	(bx)
