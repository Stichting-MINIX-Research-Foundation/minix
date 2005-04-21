!	memcmp()					Author: Kees J. Bot
!								27 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! int memcmp(const void *s1, const void *s2, size_t n)
!	Compare two chunks of memory.
!
.sect .text
.define _memcmp
_memcmp:
	cld
	push	bp
	mov	bp, sp
	push	si
	push	di
	xor	ax, ax		! Prepare return value
	mov	si, 4(bp)	! String s1
	mov	di, 6(bp)	! String s2
	mov	cx, 8(bp)	! Length
	cmp	cx, #16
	jb	cbyte		! Don't bother being smart with short arrays
	mov	dx, si
	or	dx, di
	andb	dl, #1
	jnz	cbyte		! Bit 0 set, use byte compare
cword:	sar	cx, #1
	adcb	dl, dl		! Save carry
   repe	cmps			! Compare words
	mov	cx, #2		! Recompare the last word
	sub	si, cx
	sub	di, cx
	addb	cl, dl		! One more byte?
cbyte:	test	cx, cx		! Set 'Z' flag if cx = 0
last:
   repe	cmpsb			! Look for the first differing byte
	je	equal
	ja	after
	sub	ax, #2		! if (s1 < s2) ax -= 2;
after:	inc	ax		! ax++, now it's -1 or 1
equal:	mov	dx, si		! For bcmp() to play with
	pop	di
	pop	si
	pop	bp
	ret
