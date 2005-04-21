!	strncat()					Author: Kees J. Bot
!								27 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! size_t strncat(char *s1, const char *s2, size_t n)
!	Append string s2 to s1.
!
.sect .text
.define _strncat
_strncat:
	mov	bx, sp
	mov	dx, 6(bx)	! Maximum length
	jmp	__strncat	! Common code
