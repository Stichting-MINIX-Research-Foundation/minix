!	sub64() - 64 bit subtraction			Author: Kees J. Bot
!								24 Dec 1995
.sect .text
.define _sub64

_sub64:				! u64_t sub64(u64_t i, u64_t j);
	push	bp
	mov	bp, sp
	mov	bx, 4(bp)
	mov	ax, 6(bp)
	sub	ax, 14(bp)
	mov	(bx), ax
	mov	ax, 8(bp)
	sbb	ax, 16(bp)
	mov	2(bx), ax
	mov	ax, 10(bp)
	sbb	ax, 18(bp)
	mov	4(bx), ax
	mov	ax, 12(bp)
	sbb	ax, 20(bp)
	mov	6(bx), ax
	mov	ax, bx
	pop	bp
	ret
