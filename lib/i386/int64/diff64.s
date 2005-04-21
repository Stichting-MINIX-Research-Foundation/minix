!	diff64() - 64 bit subtraction giving unsigned 	Author: Kees J. Bot
!								7 Dec 1995
.sect .text
.define _diff64

_diff64:			! unsigned diff64(u64_t i, u64_t j);
	mov	eax, 4(esp)
	sub	eax, 12(esp)
	ret

!
! $PchId: diff64.ack.s,v 1.2 1996/04/11 18:59:57 philip Exp $
