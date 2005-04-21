!	insl() - Input a dword array			Author: Kees J. Bot
!								18 Mar 1996
!	void insl(U16_t port, void *buf, size_t count);

.sect .text
.define _insl
_insl:
	push	ebp
	mov	ebp, esp
	cld
	push	edi
	mov	edx, 8(ebp)		! port
	mov	edi, 12(ebp)		! buf
	mov	ecx, 16(ebp)		! byte count
	shr	ecx, 2			! dword count
   rep	ins				! input many dwords
	pop	edi
	pop	ebp
	ret
