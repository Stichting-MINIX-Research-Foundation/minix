!	inw() - Input one word				Author: Kees J. Bot
!								18 Mar 1996
!	unsigned inw(U16_t port);

.sect .text
.define _inw
_inw:
	push	bp
	mov	bp, sp
	mov	dx, 4(bp)		! port
	in	dx			! read 1 word
	pop	bp
	ret
