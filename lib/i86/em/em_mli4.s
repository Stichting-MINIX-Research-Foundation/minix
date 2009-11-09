.define .mli4
.text

yl=2
yh=4
	! x * y
	! xl in ax
	! xh in dx

.mli4:
	mov	bx,sp
	push	dx
	mov	cx,ax
	mul	yh(bx)           ! xl*yh
	pop	dx
	push	ax
	mov	ax,dx
	mul	yl(bx)		! xh * yl
	pop	dx
	add     dx,ax           ! xh*yl+xl*yh
	mov     ax,cx
	mov	cx,dx
	mul     yl(bx)           ! xl*yl
	add     dx,cx
	ret	4
