!	_strncat()					Author: Kees J. Bot
!								27 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! char *_strncat(char *s1, const char *s2, size_t dx)
!	Append string s2 to s1.
!
.sect .text
.define __strncat
__strncat:
	push	bp
	mov	bp, sp
	push	si
	push	di
	mov	di, 4(bp)	! String s1
	mov	cx, #-1
	xorb	al, al		! Null byte
	cld
  repne	scasb			! Look for the zero byte in s1
	dec	di		! Back one up (and clear 'Z' flag)
	push	di		! Save end of s1
	mov	di, 6(bp)	! di = string s2
	mov	cx, dx		! Maximum count
  repne	scasb			! Look for the end of s2
	jne	no0
	inc	cx		! Exclude null byte
no0:	sub	dx, cx		! Number of bytes in s2
	mov	cx, dx
	mov	si, 6(bp)	! si = string s2
	pop	di		! di = end of string s1
    rep	movsb			! Copy bytes
	stosb			! Add a terminating null
	mov	ax, 4(bp)	! Return s1
	pop	di
	pop	si
	pop	bp
	ret
