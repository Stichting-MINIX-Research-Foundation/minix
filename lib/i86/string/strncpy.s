!	strncpy()					Author: Kees J. Bot
!								27 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! char *strncpy(char *s1, const char *s2, size_t n)
!	Copy string s2 to s1.
!
.sect .text
.define _strncpy
_strncpy:
	push	bp
	mov	bp, sp
	push	si
	push	di
	mov	cx, 8(bp)	! Maximum length
	call	__strncpy	! Common code
	mov	cx, dx		! Number of bytes not copied
    rep	stosb			! strncpy always copies n bytes by null padding
	mov	ax, 4(bp)	! Return s1
	pop	di
	pop	si
	pop	bp
	ret
