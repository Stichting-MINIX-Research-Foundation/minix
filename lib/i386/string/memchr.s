!	memchr()					Author: Kees J. Bot
!								2 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! void *memchr(const void *s, int c, size_t n)
!	Look for a character in a chunk of memory.
!
.sect .text
.define _memchr
	.align	16
_memchr:
	push	ebp
	mov	ebp, esp
	push	edi
	mov	edi, 8(ebp)	! edi = string
	movb	al, 12(ebp)	! The character to look for
	mov	ecx, 16(ebp)	! Length
	cmpb	cl, 1		! 'Z' bit must be clear if ecx = 0
	cld
	repne
	scasb
	jne	failure
	lea	eax, -1(edi)	! Found
	pop	edi
	pop	ebp
	ret
failure:xor	eax, eax
	pop	edi
	pop	ebp
	ret
