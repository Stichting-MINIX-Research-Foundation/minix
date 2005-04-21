!	strncpy()					Author: Kees J. Bot
!								1 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! char *strncpy(char *s1, const char *s2, size_t n)
!	Copy string s2 to s1.
!
.sect .text
.define _strncpy
	.align	16
_strncpy:
	push	ebp
	mov	ebp, esp
	push	esi
	push	edi
	mov	ecx, 16(ebp)	! Maximum length
	call	__strncpy	! Common code
	mov	ecx, edx	! Number of bytes not copied
	rep
	stosb			! strncpy always copies n bytes by null padding
	mov	eax, 8(ebp)	! Return s1
	pop	edi
	pop	esi
	pop	ebp
	ret
