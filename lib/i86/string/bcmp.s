!	bcmp()						Author: Kees J. Bot
!								27 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! int bcmp(const void *s1, const void *s2, size_t n)
!	Compare two chunks of memory.
!	This is a BSD routine that escaped from the kernel.  Don't use.
!	(Alas it is not without some use, it reports the number of bytes
!	after the bytes that are equal.  So it can't be simply replaced.)
!
.sect .text
.define _bcmp
_bcmp:
	push	bp
	mov	bp, sp
	push	8(bp)
	push	6(bp)
	push	4(bp)
	call	_memcmp		! Let memcmp do the work
	mov	sp, bp
	test	ax, ax
	jz	equal
	sub	dx, 4(bp)	! Memcmp was nice enough to leave "si" in dx
	dec	dx		! Number of bytes that are equal
	mov	ax, 8(bp)
	sub	ax, dx		! Number of bytes that are unequal
equal:	pop	bp
	ret
