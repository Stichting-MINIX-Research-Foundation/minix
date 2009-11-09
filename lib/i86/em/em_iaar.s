.define .iaar
.text

.iaar:
	pop     cx
	pop     dx
	cmp     dx,#2
.extern .unknown
	jne     .unknown
	pop     bx      ! descriptor address
	pop     ax      ! index
	sub     ax,(bx)
	mul     4(bx)
	pop     bx      ! array base
	add     bx,ax
	push	cx
	ret
