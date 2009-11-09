.define .sar2
.text

.sar2:
				! bx, descriptor address
				! ax, index
	pop	cx
	pop	dx		! base address
	push	cx
	xchg	di,dx		! di = base address, dx is saved di
	sub     ax,(bx)
	mov     cx,4(bx)
	push	dx
	imul    cx
	pop	dx
	add     di,ax
	sar     cx,#1
	jnb     1f
	pop	bx
	pop     ax
	stosb
	mov	di,dx
	jmp     (bx)
1:
	pop	bx
	mov	ax,si
	mov     si,sp
	rep
	mov
	mov     sp,si
	mov	si,ax
	mov	di,dx
	jmp     (bx)
