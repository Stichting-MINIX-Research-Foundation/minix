.sect .text; .sect .rom; .sect .data; .sect .bss
.define .csa4

.sect .text
.csa4:
				! bx, descriptor address
				! ax, dx: index
	mov	cx,(bx)         ! default
	sub     ax,2(bx)
				! ignore high order word; if non-zero, the
				! case descriptor would not fit anyway
	cmp	ax,6(bx)
	ja	1f
2:
	sal     ax,#1
	add	bx,ax
	mov     bx,10(bx)
	test    bx,bx
	jnz     2f
1:
	mov	bx,cx
	test    bx,bx
	jnz     2f
ECASE = 20
.extern .fat
	mov     ax,#ECASE
	push    ax
	jmp     .fat
2:
	jmp     (bx)
