.define .csa2

.text
.csa2:
				! bx, descriptor address
				! ax, index
	mov     dx,(bx)         ! default
	sub     ax,2(bx)
	cmp     ax,4(bx)
	ja      1f
	sal     ax,#1
	add	bx,ax
	mov     bx,6(bx)
	test    bx,bx
	jnz     2f
1:
	mov     bx,dx
	test    bx,bx
	jnz     2f
ECASE = 20
.extern .fat
	mov     ax,#ECASE
	push    ax
	jmp     .fat
2:
	jmp     (bx)
