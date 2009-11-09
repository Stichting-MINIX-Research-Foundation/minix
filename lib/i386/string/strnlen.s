!	strnlen()					Author: Kees J. Bot
!								1 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! size_t strnlen(const char *s, size_t n)
!	Return the length of a string.
!
.sect .text
.define _strnlen
	.align	16
_strnlen:
	mov	ecx, 8(esp)	! Maximum length
	jmp	__strnlen	! Common code
