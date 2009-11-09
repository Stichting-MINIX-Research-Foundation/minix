!	add64u() - unsigned to 64 bit addition		Author: Kees J. Bot
!								24 Dec 1995
.sect .text
.define _add64u, _add64ul

_add64u:			! u64_t add64u(u64_t i, unsigned j);
	push	bp
	mov	bp, sp
	xor	cx, cx
	jmp	0f
_add64ul:			! u64_t add64ul(u64_t i, unsigned long j);
	push	bp
	mov	bp, sp
	mov	cx, 16(bp)
0:	mov	bx, 4(bp)
	mov	ax, 6(bp)
	add	ax, 14(bp)
	mov	(bx), ax
	mov	ax, 8(bp)
	adc	ax, cx
	mov	2(bx), ax
	mov	ax, 10(bp)
	adc	ax, #0
	mov	4(bx), ax
	mov	ax, 12(bp)
	adc	ax, #0
	mov	6(bx), ax
	mov	ax, bx
	pop	bp
	ret
