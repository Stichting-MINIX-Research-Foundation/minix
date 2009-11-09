!	outl() - Output one dword			Author: Kees J. Bot
!								18 Mar 1996
!	void outl(U16_t port, u32_t value);

	o32 = 0x66

.sect .text
.define _outl
_outl:
	push	bp
	mov	bp, sp
	pushf
	cli				! eax is not interrupt safe
	mov	dx, 4(bp)		! port
	.data1	o32
	mov	ax, 4+2(bp)		! value
	.data1	o32
	out	dx			! output 1 dword
	popf
	pop	bp
	ret
