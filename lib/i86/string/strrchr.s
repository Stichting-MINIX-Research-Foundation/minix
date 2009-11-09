!	strrchr()					Author: Kees J. Bot
!								27 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! char *strrchr(const char *s, int c)
!	Look for the last occurrence a character in a string.
!
.sect .text
.define _strrchr
_strrchr:
	push	bp
	mov	bp, sp
	push	di
	mov	di, 4(bp)	! di = string
	mov	cx, #-1
	xorb	al, al
	cld
  repne	scasb			! Look for the end of the string
	not	cx		! -1 - cx = Length of the string + null
	dec	di		! Put di back on the zero byte
	movb	al, 6(bp)	! The character to look for
	std			! Downwards search
  repne	scasb
	cld			! Direction bit back to default
	jne	failure
	lea	ax, 1(di)	! Found it
	pop	di
	pop	bp
	ret
failure:xor	ax, ax		! Not there
	pop	di
	pop	bp
	ret
