!	outb() - Output one byte			Author: Kees J. Bot
!								18 Mar 1996
!	void outb(U16_t port, U8_t value);

.sect .text
.define _outb
_outb:
	push	ebp
	mov	ebp, esp
	mov	edx, 8(ebp)		! port
	mov	eax, 8+4(ebp)		! value
	outb	dx			! output 1 byte
	pop	ebp
	ret
