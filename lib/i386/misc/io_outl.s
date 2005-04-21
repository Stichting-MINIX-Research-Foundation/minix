!	outl() - Output one dword			Author: Kees J. Bot
!								18 Mar 1996
!	void outl(U16_t port, u32_t value);

.sect .text
.define _outl
_outl:
	push	ebp
	mov	ebp, esp
	mov	edx, 8(ebp)		! port
	mov	eax, 8+4(ebp)		! value
	out	dx			! output 1 dword
	pop	ebp
	ret
