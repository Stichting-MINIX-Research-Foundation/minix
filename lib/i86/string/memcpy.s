!	memcpy()					Author: Kees J. Bot
!								27 Jan 1994
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
_memcpy:
	push	bp
	mov	bp, sp
	push	si
	push	di
	mov	di, 4(bp)	! String s1
	mov	si, 6(bp)	! String s2
	mov	cx, 8(bp)	! Length
	! No overlap check here
	jmp	__memcpy	! Call the part of __memmove that copies up
