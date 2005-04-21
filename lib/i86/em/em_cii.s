.define .cii

.text
.cii:
	pop     bx              ! return address
				! pop     cx, dest. size
				! pop     dx, src. size
				! ax is first word of source
	cmp	dx,#1
	jne	2f
	cbw
	mov	dx,#2
2:
	cmp     dx,cx
	je      8f
	cmp     dx,#2
	je      1f
	cmp     dx,#4
	jne     9f
	cmp     cx,#2
	jne     9f
	pop     dx
8:
	jmp     (bx)
1:
	cmp     cx,#4
	jne     9f
	cwd
	push    dx
	jmp     (bx)
9:
	push    ax              ! push low source
EILLINS = 18
.extern .fat
	mov     ax,#EILLINS
	push    ax
	jmp     .fat
