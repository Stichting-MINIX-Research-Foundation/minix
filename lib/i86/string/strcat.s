!	strcat()					Author: Kees J. Bot
!								27 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! char *strcat(char *s1, const char *s2)
!	Append string s2 to s1.
!
.sect .text
.define _strcat
_strcat:
	mov	dx, #-1		! Unlimited length
	jmp	__strncat	! Common code
