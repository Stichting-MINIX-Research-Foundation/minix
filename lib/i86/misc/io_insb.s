!	insb() - Input a byte array			Author: Kees J. Bot
!								18 Mar 1996
!	void insb(U16_t port, void *buf, size_t count);

.sect .text
.define _insb
_insb:
	push	bp
	mov	bp, sp
	cld
	push	di
	mov	dx, 4(bp)		! port
	mov	di, 6(bp)		! buf
	mov	cx, 8(bp)		! byte count
	jcxz	1f
0:	inb	dx			! input 1 byte
	stosb				! write 1 byte
	loop	0b			! many times
1:	pop	di
	pop	bp
	ret
