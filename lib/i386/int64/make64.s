!	make64() - make a 64 bit number from two 32 bit halves
!							Author: Kees J. Bot
!								7 Dec 1995
.sect .text
.define _make64

_make64:		    ! u64_t make64(unsigned long lo, unsigned long hi);
	mov	eax, 4(esp)
	mov	edx, 8(esp)
	mov	(eax), edx
	mov	edx, 12(esp)
	mov	4(eax), edx
	ret

!
! $PchId: make64.ack.s,v 1.2 1996/04/11 18:59:57 philip Exp $
