!	strlen()					Author: Kees J. Bot
!								1 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! size_t strlen(const char *s)
!	Return the length of a string.
!
.sect .text
.define _strlen
	.align	16
_strlen:
	mov	ecx, -1		! Unlimited length
	jmp	__strnlen	! Common code
