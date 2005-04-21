!	sub64u() - unsigned from 64 bit subtraction	Author: Kees J. Bot
!								24 Dec 1995
.sect .text
.define _sub64u, _sub64ul

_sub64u:			! u64_t sub64u(u64_t i, unsigned j);
	push	bp
	mov	bp, sp
	xor	cx, cx
	jmp	0f
_sub64ul:			! u64_t sub64ul(u64_t i, unsigned long j);
	push	bp
	mov	bp, sp
	mov	cx, 16(bp)
0:	mov	bx, 4(bp)
	mov	ax, 6(bp)
	sub	ax, 14(bp)
	mov	(bx), ax
	mov	ax, 8(bp)
	sbb	ax, cx
	mov	2(bx), ax
	mov	ax, 10(bp)
	sbb	ax, #0
	mov	4(bx), ax
	mov	ax, 12(bp)
	sbb	ax, #0
	mov	6(bx), ax
	mov	ax, bx
	pop	bp
	ret
