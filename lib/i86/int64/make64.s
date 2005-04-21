!	make64() - make a 64 bit number from two 32 bit halves
!							Author: Kees J. Bot
!								24 Dec 1995
.sect .text
.define _make64

_make64:		    ! u64_t make64(unsigned long lo, unsigned long hi);
	mov	bx, sp
	mov	ax, 4(bx)
	mov	dx, 6(bx)
	mov	cx, 8(bx)
	push	10(bx)
	mov	bx, 2(bx)
	mov	(bx), ax
	mov	2(bx), dx
	mov	4(bx), cx
	pop	6(bx)
	mov	ax, bx
	ret
