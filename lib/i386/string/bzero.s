!	bzero()						Author: Kees J. Bot
!								2 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! void bzero(void *s, size_t n)
!	Set a chunk of memory to zero.
!	This is a BSD routine that escaped from the kernel.  Don't use.
!
.sect .text
.define _bzero
	.align	16
_bzero:
	push	ebp
	mov	ebp, esp
	push	12(ebp)		! Size
	push	0		! Zero
	push	8(ebp)		! String
	call	_memset		! Call the proper routine
	leave
	ret
