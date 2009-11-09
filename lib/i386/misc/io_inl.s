!	inl() - Input one dword				Author: Kees J. Bot
!								18 Mar 1996
!	unsigned inl(U16_t port);

.sect .text
.define _inl
_inl:
	push	ebp
	mov	ebp, esp
	mov	edx, 8(ebp)		! port
	in	dx			! read 1 dword
	pop	ebp
	ret
