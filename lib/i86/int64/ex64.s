!	ex64*() - extract low or high 32 bits of a 64 bit number
!							Author: Kees J. Bot
!								24 Dec 1995
.sect .text
.define _ex64lo, _ex64hi

_ex64lo:			! unsigned long ex64lo(u64_t i);
	mov	bx, sp
	mov	ax, 2(bx)
	mov	dx, 4(bx)
	ret

_ex64hi:			! unsigned long ex64hi(u64_t i);
	mov	bx, sp
	mov	ax, 6(bx)
	mov	dx, 8(bx)
	ret
