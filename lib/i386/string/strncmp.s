!	strncmp()					Author: Kees J. Bot
!								1 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! int strncmp(const char *s1, const char *s2, size_t n)
!	Compare two strings.
!
.sect .text
.define _strncmp
	.align	16
_strncmp:
	mov	ecx, 12(esp)	! Maximum length
	jmp	__strncmp	! Common code
