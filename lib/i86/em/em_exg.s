.define .exg

	! #bytes in cx
.text
.exg:
	push	di
	mov	sp,di
	add	di,#4
	mov	bx,di
	add	bx,cx
	sar     cx,#1
1:
	mov	ax,(bx)
	xchg	ax,(di)
	mov	(bx),ax
	add	di,#2
	add	bx,#2
	loop	1b
2:
	pop	di
	ret
