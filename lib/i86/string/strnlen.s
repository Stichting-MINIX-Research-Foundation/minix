!	strnlen()					Author: Kees J. Bot
!								27 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! size_t strnlen(const char *s, size_t n)
!	Return the length of a string.
!
.sect .text
.define _strnlen
_strnlen:
	mov	bx, sp
	mov	cx, 4(bx)	! Maximum length
	jmp	__strnlen	! Common code
