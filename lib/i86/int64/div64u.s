!	div64u() - 64 bit divided by unsigned giving unsigned long
!							Author: Kees J. Bot
!								24 Dec 1995
.sect .text
.define _div64u, _rem64u

_div64u:			! unsigned long div64u(u64_t i, unsigned j);
	mov	bx, sp
div64u:	xor	dx, dx
	mov	ax, 8(bx)
	div	10(bx)
	mov	ax, 6(bx)
	div	10(bx)
	mov	ax, 4(bx)
	div	10(bx)			! division bits 16-31
	mov	cx, ax
	mov	ax, 2(bx)
	div	10(bx)			! division bits 0-15
	xchg	dx, cx			! division in dx:ax, remainder in cx
	ret

_rem64u:			! unsigned rem64u(u64_t i, unsigned j);
	mov	bx, sp
	call	div64u
	mov	ax, cx
	ret
