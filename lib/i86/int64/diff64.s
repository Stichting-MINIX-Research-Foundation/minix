!	diff64() - 64 bit subtraction giving unsigned 	Author: Kees J. Bot
!								24 Dec 1995
.sect .text
.define _diff64

_diff64:			! unsigned diff64(u64_t i, u64_t j);
	mov	bx, sp
	mov	ax, 2(bx)
	sub	ax, 10(bx)
	ret
