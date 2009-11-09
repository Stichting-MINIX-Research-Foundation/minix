!	_strnlen()					Author: Kees J. Bot
!								1 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! size_t _strnlen(const char *s, size_t ecx)
!	Return the length of a string.
!
.sect .text
.define __strnlen
	.align	16
__strnlen:
	push	ebp
	mov	ebp, esp
	push	edi
	mov	edi, 8(ebp)	! edi = string
	xorb	al, al		! Look for a zero byte
	mov	edx, ecx	! Save maximum count
	cmpb	cl, 1		! 'Z' bit must be clear if ecx = 0
	cld
	repne
	scasb			! Look for zero
	jne	no0
	inc	ecx		! Don't count zero byte
no0:	mov	eax, edx
	sub	eax, ecx	! Compute bytes scanned
	pop	edi
	pop	ebp
	ret
