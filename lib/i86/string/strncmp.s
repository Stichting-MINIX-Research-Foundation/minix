!	strncmp()					Author: Kees J. Bot
!								27 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! int strncmp(const char *s1, const char *s2, size_t n)
!	Compare two strings.
!
.sect .text
.define _strncmp
_strncmp:
	mov	bx, sp
	mov	cx, 6(bx)	! Maximum length
	jmp	__strncmp	! Common code
