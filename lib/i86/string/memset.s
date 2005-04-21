!	memset()					Author: Kees J. Bot
!								27 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! void *memset(void *s, int c, size_t n)
!	Set a chunk of memory to the same byte value.
!
.sect .text
.define _memset
_memset:
	push	bp
	mov	bp, sp
	push	di
	mov	di, 4(bp)	! The string
	movb	al, 6(bp)	! The fill byte
	mov	cx, 8(bp)	! Length
	cld
	cmp	cx, #16
	jb	sbyte		! Don't bother being smart with short arrays
	test	di, #1
	jnz	sbyte		! Bit 0 set, use byte store
sword:	movb	ah, al		! One byte to two bytes
	sar	cx, #1
    rep	stos			! Store words
    	adc	cx, cx		! One more byte?
sbyte:
    rep	stosb			! Store bytes
done:	mov	ax, 4(bp)	! Return some value you have no need for
	pop	di
	pop	bp
	ret
