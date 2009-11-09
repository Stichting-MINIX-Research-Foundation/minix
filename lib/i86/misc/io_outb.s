!	outb() - Output one byte			Author: Kees J. Bot
!								18 Mar 1996
!	void outb(U16_t port, U8_t value);

.sect .text
.define _outb
_outb:
	push	bp
	mov	bp, sp
	mov	dx, 4(bp)		! port
	mov	ax, 4+2(bp)		! value
	outb	dx			! output 1 byte
	pop	bp
	ret
