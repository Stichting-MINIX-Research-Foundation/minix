!	index()						Author: Kees J. Bot
!								2 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! char *index(const char *s, int c)
!	Look for a character in a string.  Has suffered from a hostile
!	takeover by strchr().
!
.sect .text
.define _index
	.align	16
_index:
	jmp	_strchr
