.define .dvi4

yl=6
yh=8
xl=10
xh=12

.text
.dvi4:
	push	si
	push	di
	mov     si,sp           ! copy of sp
	mov     bx,yl(si)
	mov     ax,yh(si)
	cwd
	mov     di,dx
	cmp     dx,ax
	jne     7f
	and     dx,dx
	jge     1f
	neg     bx
	je      7f
1:
	xor     dx,dx
	mov     cx,xl(si)
	mov     ax,xh(si)
	and     ax,ax
	jge     2f
	neg     ax
	neg     cx
	sbb     ax,dx
	not     di
2:
	div     bx
	xchg    ax,cx
	div     bx              ! cx = high abs(result), ax=low abs(result)
9:
	and     di,di
	jge     1f
	neg     cx
	neg     ax
	sbb     cx,#0
1:
			! cx is high order result
			! ax is low order result
	mov	dx,cx
	pop	di
	pop	si
	ret	8	! result in ax/dx

7:
	push    dx              ! sign of y
	mov     di,ax
	xor     bx,bx
	and     di,di
	jge     1f
	neg     di
	neg     yl(si)
	sbb     di,bx
1:
	mov     ax,xl(si)
	mov     dx,xh(si)
	and     dx,dx
	jge     1f
	neg     dx
	neg     ax
	sbb     dx,bx
	not     -2(si)
1:
	mov     cx,#16
1:
	shl     ax,#1
	rcl     dx,#1
	rcl     bx,#1
	cmp     di,bx
	ja      3f
	jb      2f
	cmp     yl(si),dx
	jbe     2f
3:
	loop    1b
	jmp     1f
2:
	sub     dx,yl(si)
	sbb     bx,di
	inc     ax
	loop    1b
1:
	pop     di              ! di=sign of result,ax= result
	jmp     9b
