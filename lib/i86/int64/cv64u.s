!	cv64u() - 64 bit converted to unsigned		Author: Kees J. Bot
!								24 Dec 1995
.sect .text
.define _cv64u, _cv64ul

_cv64u:				! unsigned cv64u(u64_t i);
	mov	bx, sp
	mov	cx, 4(bx)
	jmp	0f

_cv64ul:			! unsigned long cv64ul(u64_t i);
	mov	bx, sp
	xor	cx, cx
0:	mov	ax, 2(bx)
	mov	dx, 4(bx)
	or	cx, 6(bx)
	or	cx, 8(bx)		! return UINT/ULONG_MAX if really big
	jz	0f
	mov	ax, #-1
	mov	dx, ax
0:	ret
