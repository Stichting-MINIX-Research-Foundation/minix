!	inb() - Input one byte				Author: Kees J. Bot
!								18 Mar 1996
!	unsigned inb(U16_t port);

.sect .text
.define _inb
_inb:
	push	bp
	mov	bp, sp
	mov	dx, 4(bp)		! port
	inb	dx			! read 1 byte
	xorb	ah, ah
	pop	bp
	ret
