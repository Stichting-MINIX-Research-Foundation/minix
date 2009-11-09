.sect .text; .sect .rom; .sect .data; .sect .bss
.define .csb4

.sect .text
.csb4:
				!bx: descriptor address
				!ax, dx:  index
	push	(bx)		! default
	mov	cx,2(bx)	! count (ignore high order word, the descriptor
				! would not fit anyway)
1:
	add	bx,#6
	dec     cx
	jl      4f
	cmp     ax,(bx)
	jne     1b
	cmp	dx,2(bx)
	jne     1b
	pop	bx
	mov	bx,4(bx)
2:
	test    bx,bx
	jnz     3f
ECASE = 20
.extern .fat
	mov     ax,#ECASE
	push    ax
	jmp     .fat
3:
	jmp	(bx)
4:
	pop	bx
	jmp	2b
