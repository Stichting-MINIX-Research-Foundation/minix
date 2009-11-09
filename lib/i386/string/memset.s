!	memset()					Author: Kees J. Bot
!								2 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! void *memset(void *s, int c, size_t n)
!	Set a chunk of memory to the same byte value.
!
.sect .text
.define _memset
	.align	16
_memset:
	push	ebp
	mov	ebp, esp
	push	edi
	mov	edi, 8(ebp)	! The string
	movzxb	eax, 12(ebp)	! The fill byte
	mov	ecx, 16(ebp)	! Length
	cld
	cmp	ecx, 16
	jb	sbyte		! Don't bother being smart with short arrays
	test	edi, 1
	jnz	sbyte		! Bit 0 set, use byte store
	test	edi, 2
	jnz	sword		! Bit 1 set, use word store
slword:	movb	ah, al
	mov	edx, eax
	sal	edx, 16
	or	eax, edx	! One byte to four bytes
	shrd	edx, ecx, 2	! Save low two bits of ecx in edx
	shr	ecx, 2
	rep
	stos			! Store longwords.
	shld	ecx, edx, 2	! Restore low two bits
sword:	movb	ah, al		! One byte to two bytes
	shr	ecx, 1
	rep
    o16	stos			! Store words
	adc	ecx, ecx	! One more byte?
sbyte:	rep
	stosb			! Store bytes
done:	mov	eax, 8(ebp)	! Return some value you have no need for
	pop	edi
	pop	ebp
	ret
