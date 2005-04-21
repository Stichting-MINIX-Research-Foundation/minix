!	_strncpy()					Author: Kees J. Bot
!								1 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! char *_strncpy(char *s1, const char *s2, size_t ecx)
!	Copy string s2 to s1.
!
.sect .text
.define __strncpy
	.align	16
__strncpy:
	mov	edi, 12(ebp)	! edi = string s2
	xorb	al, al		! Look for a zero byte
	mov	edx, ecx	! Save maximum count
	cld
	repne
	scasb			! Look for end of s2
	sub	edx, ecx	! Number of bytes in s2 including null
	xchg	ecx, edx
	mov	esi, 12(ebp)	! esi = string s2
	mov	edi, 8(ebp)	! edi = string s1
	rep
	movsb			! Copy bytes
	ret
