!	_strncpy()					Author: Kees J. Bot
!								27 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! char *_strncpy(char *s1, const char *s2, size_t cx)
!	Copy string s2 to s1.
!
.sect .text
.define __strncpy
__strncpy:
	mov	di, 6(bp)	! di = string s2
	xorb	al, al		! Look for a zero byte
	mov	dx, cx		! Save maximum count
	cld
  repne	scasb			! Look for end of s2
	sub	dx, cx		! Number of bytes in s2 including null
	xchg	cx, dx
	mov	si, 6(bp)	! si = string s2
	mov	di, 4(bp)	! di = string s1
    rep	movsb			! Copy bytes
	ret
