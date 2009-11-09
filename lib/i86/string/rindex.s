!	rindex()					Author: Kees J. Bot
!								27 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! char *rindex(const char *s, int c)
!	Look for the last occurrence a character in a string.  Has suffered
!	from a hostile takeover by strrchr().
!
.sect .text
.define _rindex
_rindex:
	jmp	_strrchr
