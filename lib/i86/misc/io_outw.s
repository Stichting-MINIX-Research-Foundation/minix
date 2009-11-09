!	outw() - Output one word			Author: Kees J. Bot
!								18 Mar 1996
!	void outw(U16_t port, U16_t value);

.sect .text
.define _outw
_outw:
	push	bp
	mov	bp, sp
	mov	dx, 4(bp)		! port
	mov	ax, 4+2(bp)		! value
	out	dx			! output 1 word
	pop	bp
	ret
