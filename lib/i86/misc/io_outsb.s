!	outsb() - Output a byte array		Author: Kees J. Bot
!								18 Mar 1996
!	void outsb(U16_t port, void *buf, size_t count);

.sect .text
.define _outsb
_outsb:
	push	bp
	mov	bp, sp
	cld
	push	si
	mov	dx, 4(bp)		! port
	mov	si, 6(bp)		! buf
	mov	cx, 8(bp)		! byte count
	jcxz	1f
0:	lodsb				! read 1 byte
	outb	dx			! output 1 byte
	loop	0b			! many times
1:	pop	si
	pop	bp
	ret
