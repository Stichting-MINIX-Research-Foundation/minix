!	_strncat()					Author: Kees J. Bot
!								1 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! char *_strncat(char *s1, const char *s2, size_t edx)
!	Append string s2 to s1.
!
.sect .text
.define __strncat
	.align	16
__strncat:
	push	ebp
	mov	ebp, esp
	push	esi
	push	edi
	mov	edi, 8(ebp)	! String s1
	mov	ecx, -1
	xorb	al, al		! Null byte
	cld
	repne
	scasb			! Look for the zero byte in s1
	dec	edi		! Back one up (and clear 'Z' flag)
	push	edi		! Save end of s1
	mov	edi, 12(ebp)	! edi = string s2
	mov	ecx, edx	! Maximum count
	repne
	scasb			! Look for the end of s2
	jne	no0
	inc	ecx		! Exclude null byte
no0:	sub	edx, ecx	! Number of bytes in s2
	mov	ecx, edx
	mov	esi, 12(ebp)	! esi = string s2
	pop	edi		! edi = end of string s1
	rep
	movsb			! Copy bytes
	stosb			! Add a terminating null
	mov	eax, 8(ebp)	! Return s1
	pop	edi
	pop	esi
	pop	ebp
	ret
