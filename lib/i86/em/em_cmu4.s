.define .cmu4

.text
.cmu4:
	pop     bx              ! return address
	pop     cx
	pop     dx
	pop     ax
	push	si
	mov	si,sp
	xchg	bx,2(si)
	pop	si
	cmp     bx,dx
	ja      1f
	jb      2f
	cmp     ax,cx
	ja      1f
	je      3f
2:
	mov	ax,#-1
	ret
3:
	xor	ax,ax
	ret
1:
	mov	ax,#1
	ret
