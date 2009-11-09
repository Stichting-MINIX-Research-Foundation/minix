!	strchr()					Author: Kees J. Bot
!								27 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! char *strchr(const char *s, int c)
!	Look for a character in a string.
!
.sect .text
.define _strchr
_strchr:
	push	bp
	mov	bp, sp
	push	di
	cld
	mov	di, 4(bp)	! di = string
	mov	dx, #16		! Look at small chunks of the string
next:	shl	dx, #1		! Chunks become bigger each time
	mov	cx, dx
	xorb	al, al		! Look for the zero at the end
  repne	scasb
	pushf			! Remember the flags
	sub	cx, dx
	neg	cx		! Some or all of the chunk
	sub	di, cx		! Step back
	movb	al, 6(bp)	! The character to look for
  repne	scasb
	je	found
	popf			! Did we find the end of string earlier?
	jne	next		! No, try again
	xor	ax, ax		! Return NULL
	pop	di
	pop	bp
	ret
found:	pop	ax		! Get rid of those flags
	lea	ax, -1(di)	! Address of byte found
	pop	di
	pop	bp
	ret
