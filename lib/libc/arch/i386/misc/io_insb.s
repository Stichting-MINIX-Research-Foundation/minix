!	insb() - Input a byte array			Author: Kees J. Bot
!								18 Mar 1996
!	void insb(U16_t port, void *buf, size_t count);

.sect .text
.define _insb
_insb:
	push	ebp
	mov	ebp, esp
	cld
	push	edi
	mov	edx, 8(ebp)		! port
	mov	edi, 12(ebp)		! buf
	mov	ecx, 16(ebp)		! byte count
   rep	insb				! input many bytes
	pop	edi
	pop	ebp
	ret
