.define .loi
.define .los
.text

	! #bytes in cx
	! address in bx
	! save si/di. they might be register variables
.los:
	mov	dx,si
	mov	si,bx
	pop	bx
	mov	ax,cx
	sar	cx,#1
	jnb	1f
	xorb	ah,ah
	lodsb
	mov	si,dx
	push	ax
	jmp	(bx)
1:
	sub	sp,ax
	jmp	1f

.loi:
	! only called with size > 4
	mov	dx,si
	mov	si,bx
	pop     bx
	sub     sp,cx
	sar     cx,#1
1:
	mov	ax,di
	mov     di,sp
	rep
	mov
	mov	si,dx
	mov	di,ax
	jmp     (bx)
