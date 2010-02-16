!	bcmp()						Author: Kees J. Bot
!								2 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! int bcmp(const void *s1, const void *s2, size_t n)
!	Compare two chunks of memory.
!	This is a BSD routine that escaped from the kernel.  Don't use.
!	(Alas it is not without some use, it reports the number of bytes
!	after the bytes that are equal.  So it can't be simply replaced.)
!
.sect .text
.define _bcmp
	.align	16
_bcmp:
	push	ebp
	mov	ebp, esp
	push	16(ebp)
	push	12(ebp)
	push	8(ebp)
	call	_memcmp		! Let memcmp do the work
	test	eax, eax
	jz	equal
	sub	edx, 8(ebp)	! Memcmp was nice enough to leave "esi" in edx
	dec	edx		! Number of bytes that are equal
	mov	eax, 16(ebp)
	sub	eax, edx	! Number of bytes that are unequal
equal:	leave
	ret
