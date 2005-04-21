!	insl() - Input a dword array			Author: Kees J. Bot
!								18 Mar 1996
!	void insl(U16_t port, void *buf, size_t count);

	o32 = 0x66

.sect .text
.define _insl
_insl:
	push	bp
	mov	bp, sp
	cld
	push	di
	mov	dx, 4(bp)		! port
	mov	di, 6(bp)		! buf
	mov	cx, 8(bp)		! byte count
	shr	cx, #2			! dword count
	.data1	o32
   rep	ins				! input many dwords
	pop	di
	pop	bp
	ret
