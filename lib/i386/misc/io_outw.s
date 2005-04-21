!	outw() - Output one word			Author: Kees J. Bot
!								18 Mar 1996
!	void outw(U16_t port, U16_t value);

.sect .text
.define _outw
_outw:
	push	ebp
	mov	ebp, esp
	mov	edx, 8(ebp)		! port
	mov	eax, 8+4(ebp)		! value
    o16	out	dx			! output 1 word
	pop	ebp
	ret
