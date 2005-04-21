.define .sdret, .dsret, .sret, .dret, .cret 
.text

.dsret:
	pop	di
.sret:
	pop	si
.cret:
	mov	sp,bp
	pop	bp
	ret

.sdret:
	pop	si
.dret:
	pop	di
	jmp	.cret
