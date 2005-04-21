!	sub64() - unsigned from 64 bit subtraction	Author: Kees J. Bot
!								7 Dec 1995
.sect .text
.define _sub64u, _sub64ul

_sub64u:			! u64_t sub64u(u64_t i, unsigned j);
_sub64ul:			! u64_t sub64ul(u64_t i, unsigned long j);
	mov	eax, 4(esp)
	mov	edx, 8(esp)
	sub	edx, 16(esp)
	mov	(eax), edx
	mov	edx, 12(esp)
	sbb	edx, 0
	mov	4(eax), edx
	ret

!
! $PchId: sub64u.ack.s,v 1.2 1996/04/11 18:59:57 philip Exp $
