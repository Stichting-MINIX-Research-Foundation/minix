!	outsw() - Output a word array		Author: Kees J. Bot
!								18 Mar 1996
!	void outsw(U16_t port, void *buf, size_t count);

.sect .text
.define _outsw
_outsw:
	push	ebp
	mov	ebp, esp
	cld
	push	esi
	mov	edx, 8(ebp)		! port
	mov	esi, 12(ebp)		! buf
	mov	ecx, 16(ebp)		! byte count
	shr	ecx, 1			! word count
rep o16	outs				! output many words
	pop	esi
	pop	ebp
	ret
