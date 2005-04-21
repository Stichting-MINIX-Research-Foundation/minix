!	memcpy()					Author: Kees J. Bot
!								2 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! void *memcpy(void *s1, const void *s2, size_t n)
!	Copy a chunk of memory.
!	This routine need not handle overlap, so it does not handle overlap.
!	One could simply call __memmove, the cost of the overlap check is
!	negligible, but you are dealing with a programmer who believes that
!	if anything can go wrong, it should go wrong.
!
.sect .text
.define _memcpy
	.align	16
_memcpy:
	push	ebp
	mov	ebp, esp
	push	esi
	push	edi
	mov	edi, 8(ebp)	! String s1
	mov	esi, 12(ebp)	! String s2
	mov	ecx, 16(ebp)	! Length
	! No overlap check here
	jmp	__memcpy	! Call the part of __memmove that copies up
