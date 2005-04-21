!	bcopy()						Author: Kees J. Bot
!								27 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! void bcopy(const void *s1, void *s2, size_t n)
!	Copy a chunk of memory.  Handle overlap.
!	This is a BSD routine that escaped from the kernel.  Don't use.
!
.sect .text
.define _bcopy
.extern __memmove
_bcopy:
	pop	cx
	pop	ax
	pop	dx		! Pop return address and arguments
	push	ax
	push	dx		! Arguments reversed
	push	cx
	jmp	__memmove	! Call the proper routine
