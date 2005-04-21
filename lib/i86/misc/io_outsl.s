!	outsl() - Output a dword array		Author: Kees J. Bot
!								18 Mar 1996
!	void outsl(U16_t port, void *buf, size_t count);

	o32 = 0x66

.sect .text
.define _outsl
_outsl:
	push	bp
	mov	bp, sp
	cld
	push	si
	mov	dx, 4(bp)		! port
	mov	si, 6(bp)		! buf
	mov	cx, 8(bp)		! byte count
	shr	cx, #2			! dword count
	.data1	o32
   rep	outs				! output many dwords
	pop	si
	pop	bp
	ret
