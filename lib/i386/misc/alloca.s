#
!	alloca() - allocate space on the stack		Author: Kees J. Bot
!								2 Dec 1993
.sect .text; .sect .rom; .sect .data; .sect .bss

.sect .text
	.align	16
.define _alloca
_alloca:
#if __ACK__
	pop	ecx		! Return address
	pop	eax		! Bytes to allocate
	add	eax, 2*4+3	! Add space for two saved register variables
	andb	al, 0xFC	! Align
	mov	ebx, esp	! Keep current esp
	sub	esp, eax	! Lower stack
	mov	eax, esp	! Return value
	push	4(ebx)		! Push what is probably the saved esi
	push	(ebx)		! Saved edi
				! Now ACK can still do:
				!	pop edi; pop esi; leave; ret
	push	eax		! Dummy argument
	jmp	ecx
#else
	pop	ecx		! Return address
	pop	eax		! Bytes to allocate
	add	eax, 3
	andb	al, 0xFC	! Align
	sub	esp, eax	! Lower stack
	mov	eax, esp	! Return value
	push	eax		! Dummy argument
	jmp	ecx
#endif
