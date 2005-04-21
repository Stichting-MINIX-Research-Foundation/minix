!	strcpy()					Author: Kees J. Bot
!								27 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! char *strcpy(char *s1, const char *s2)
!	Copy string s2 to s1.
!
.sect .text
.define _strcpy
_strcpy:
	push	bp
	mov	bp, sp
	push	si
	push	di
	mov	cx, #-1		! Unlimited length
	call	__strncpy	! Common code
	mov	ax, 4(bp)	! Return s1
	pop	di
	pop	si
	pop	bp
	ret
