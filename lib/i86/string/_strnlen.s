!	_strnlen()					Author: Kees J. Bot
!								27 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! size_t _strnlen(const char *s, size_t cx)
!	Return the length of a string.
!
.sect .text
.define __strnlen
__strnlen:
	push	bp
	mov	bp, sp
	push	di
	mov	di, 4(bp)	! di = string
	xorb	al, al		! Look for a zero byte
	mov	dx, cx		! Save maximum count
	cmpb	cl, #1		! 'Z' bit must be clear if cx = 0
	cld
  repne	scasb			! Look for zero
	jne	no0
	inc	cx		! Don't count zero byte
no0:	mov	ax, dx
	sub	ax, cx		! Compute bytes scanned
	pop	di
	pop	bp
	ret
