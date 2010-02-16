!	bcopy()						Author: Kees J. Bot
!								2 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! void bcopy(const void *s1, void *s2, size_t n)
!	Copy a chunk of memory.  Handle overlap.
!	This is a BSD routine that escaped from the kernel.  Don't use.
!
.sect .text
.define _bcopy
	.align	16
_bcopy:
	mov	eax, 4(esp)	! Exchange string arguments
	xchg	eax, 8(esp)
	mov	4(esp), eax
	jmp	__memmove	! Call the proper routine
