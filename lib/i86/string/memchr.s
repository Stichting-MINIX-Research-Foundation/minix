!	memchr()					Author: Kees J. Bot
!								27 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! void *memchr(const void *s, int c, size_t n)
!	Look for a character in a chunk of memory.
!
.sect .text
.define _memchr
_memchr:
	push	bp
	mov	bp, sp
	push	di
	mov	di, 4(bp)	! di = string
	movb	al, 6(bp)	! The character to look for
	mov	cx, 8(bp)	! Length
	cmpb	cl, #1		! 'Z' bit must be clear if cx = 0
	cld
  repne	scasb
	jne	failure
	lea	ax, -1(di)	! Found
	pop	di
	pop	bp
	ret
failure:xor	ax, ax
	pop	di
	pop	bp
	ret
