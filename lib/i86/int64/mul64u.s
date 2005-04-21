!	mul64u() - unsigned long by unsigned multiply giving 64 bit result
!							Author: Kees J. Bot
!								24 Dec 1995
.sect .text
.define _mul64u

_mul64u:			! u64_t mul64u(unsigned long i, unsigned j);
	push	bp
	mov	bp, sp
	mov	bx, 4(bp)
	mov	ax, 6(bp)
	mul	10(bp)
	mov	(bx), ax
	mov	2(bx), dx
	mov	ax, 8(bp)
	mul	10(bp)
	add	2(bx), ax
	adc	dx, #0
	mov	4(bx), dx
	mov	6(bx), #0
	mov	ax, bx
	pop	bp
	ret
