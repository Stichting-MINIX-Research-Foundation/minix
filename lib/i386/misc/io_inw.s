!	inw() - Input one word				Author: Kees J. Bot
!								18 Mar 1996
!	unsigned inw(U16_t port);

.sect .text
.define _inw
_inw:
	push	ebp
	mov	ebp, esp
	mov	edx, 8(ebp)		! port
	xor	eax, eax
    o16	in	dx			! read 1 word
	pop	ebp
	ret
