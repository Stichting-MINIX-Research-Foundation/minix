.define .blm
.text

	! cx: count in words
.blm:
	mov	bx,sp
	mov	ax,si
	mov	dx,di
	mov	di,2(bx)
	mov	si,4(bx)
	rep
	mov
	mov	si,ax
	mov	di,dx
	ret	4
