!	outsw() - Output a word array		Author: Kees J. Bot
!								18 Mar 1996
!	void outsw(U16_t port, void *buf, size_t count);

.sect .text
.define _outsw
_outsw:
	push	bp
	mov	bp, sp
	cld
	push	si
	mov	dx, 4(bp)		! port
	mov	si, 6(bp)		! buf
	mov	cx, 8(bp)		! byte count
	shr	cx, #1			! word count
   rep	outs				! output many words
	pop	si
	pop	bp
	ret
