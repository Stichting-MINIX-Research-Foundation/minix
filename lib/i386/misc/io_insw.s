!	insw() - Input a word array			Author: Kees J. Bot
!								18 Mar 1996
!	void insw(U16_t port, void *buf, size_t count);

.sect .text
.define _insw
_insw:
	push	ebp
	mov	ebp, esp
	cld
	push	edi
	mov	edx, 8(ebp)		! port
	mov	edi, 12(ebp)		! buf
	mov	ecx, 16(ebp)		! byte count
	shr	ecx, 1			! word count
rep o16	ins				! input many words
	pop	edi
	pop	ebp
	ret
