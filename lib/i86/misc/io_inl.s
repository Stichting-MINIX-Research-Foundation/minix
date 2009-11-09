!	inl() - Input one dword				Author: Kees J. Bot
!								18 Mar 1996
!	unsigned inl(U16_t port);

	o32 = 0x66

.sect .text
.define _inl
_inl:
	push	bp
	mov	bp, sp
	pushf
	cli				! eax is not interrupt safe
	mov	dx, 4(bp)		! port
	.data1	o32
	in	dx			! read 1 dword
	.data1	o32
	push	ax			! push eax
	pop	ax
	pop	dx			! dx:ax = eax
	popf
	pop	bp
	ret
