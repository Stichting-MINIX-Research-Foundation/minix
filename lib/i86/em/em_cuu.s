.define .ciu
.define .cui
.define .cuu

.text
.ciu:
.cui:
.cuu:
	pop     bx              ! return address
				! pop     cx, dest. size
				! pop     dx, source size
				! ax is low word of source
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
	xor     dx,dx
	push    dx
	jmp     (bx)
9:
	push    ax              ! to help debugging ?
EILLINS = 18
.extern .fat
	mov     ax,#EILLINS
	push    ax
	jmp     .fat
