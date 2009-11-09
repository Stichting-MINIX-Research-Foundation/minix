.define .adi

	.text
.adi:
	pop     bx 
	cmp     cx,#2
	jne     1f
	pop     cx
	add     ax,cx
	jmp     (bx)
1:
	cmp     cx,#4
	jne     9f
	pop     dx
	pop     cx
	add     ax,cx
	pop     cx
	adc     dx,cx
	push    dx
	jmp     (bx)
9:
.extern .trpilin
	push	bx
	jmp     .trpilin
