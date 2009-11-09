.define .lar2
.text

.lar2:
				! bx, descriptor address
				! ax, index
	pop	cx
	pop	dx		! base address
	push	cx
	push	si
	mov	si,dx
	sub     ax,(bx)
	mov     cx,4(bx)
	imul    cx
	add     si,ax
	sar     cx,#1
	jnb     1f
	xorb    ah,ah
	lodsb
	pop	si
	pop	bx
	push    ax
	jmp     (bx)
1:
	pop	dx		! saved si
	mov	ax,4(bx)
	pop	bx		! return address
	sub     sp,ax
	mov	ax,di		! save di
	mov     di,sp
	rep
	mov
	mov	di,ax
	mov	si,dx
	jmp     (bx)
