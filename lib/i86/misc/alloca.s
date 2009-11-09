#
!	alloca() - allocate space on the stack		Author: Kees J. Bot
!								26 Jan 1994

#if __ACK__	/* BCC can't do alloca(), register saving is wrong. */

.text
.define _alloca
_alloca:
	pop	cx		! Return address
	pop	ax		! Bytes to allocate
	add	ax, #2*2+1	! Add space for two saved register variables
	andb	al, #0xFE	! Align
	mov	bx, sp		! Keep current sp
	sub	sp, ax		! Lower stack
	mov	ax, sp		! Return value
	push	2(bx)		! Push what is probably the saved si
	push	(bx)		! Saved di
				! Now ACK can still do:
				!	pop di; pop si; mov sp, bp; pop bp; ret
	push	ax		! Dummy argument
	jmp	(cx)
#endif
