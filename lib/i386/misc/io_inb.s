!	inb() - Input one byte				Author: Kees J. Bot
!								18 Mar 1996
!	unsigned inb(U16_t port);

.sect .text
.define _inb
_inb:
	push	ebp
	mov	ebp, esp
	mov	edx, 8(ebp)		! port
	xor	eax, eax
	inb	dx			! read 1 byte
	pop	ebp
	ret
