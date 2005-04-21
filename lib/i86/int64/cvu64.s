!	cvu64() - unsigned converted to 64 bit		Author: Kees J. Bot
!								24 Dec 1995
.sect .text
.define _cvu64, _cvul64

_cvu64:				! u64_t cvu64(unsigned i);
	mov	bx, sp
	xor	dx, dx
	jmp	0f

_cvul64:			! u64_t cvul64(unsigned long i);
	mov	bx, sp
	mov	dx, 6(bx)
0:	mov	ax, 4(bx)
	mov	bx, 2(bx)
	mov	(bx), ax
	mov	2(bx), dx
	mov	4(bx), #0
	mov	6(bx), #0
	mov	ax, bx
	ret
