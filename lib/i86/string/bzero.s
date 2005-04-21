!	bzero()						Author: Kees J. Bot
!								27 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! void bzero(void *s, size_t n)
!	Set a chunk of memory to zero.
!	This is a BSD routine that escaped from the kernel.  Don't use.
!
.sect .text
.define _bzero
_bzero:
	push	bp
	mov	bp, sp
	push	6(bp)		! Size
	xor	ax, ax
	push	ax		! Zero
	push	4(bp)		! String
	call	_memset		! Call the proper routine
	mov	sp, bp
	pop	bp
	ret
