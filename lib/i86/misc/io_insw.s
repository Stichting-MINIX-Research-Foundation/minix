!	insw() - Input a word array			Author: Kees J. Bot
!								18 Mar 1996
!	void insw(U16_t port, void *buf, size_t count);

.sect .text
.define _insw
_insw:
	push	bp
	mov	bp, sp
	cld
	push	di
	mov	dx, 4(bp)		! port
	mov	di, 6(bp)		! buf
	mov	cx, 8(bp)		! byte count
	shr	cx, #1			! word count
   rep	ins				! input many words
	pop	di
	pop	bp
	ret
