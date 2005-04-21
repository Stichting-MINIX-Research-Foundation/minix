.define ___stb
.text

	! Routine for copying structs.
___stb:
	mov	bx,sp
	push	si
	push	di
	mov	cx,2(bx)
	mov	si,4(bx)
	mov	di,6(bx)
	rep
	movb
	pop	di
	pop	si
	ret
