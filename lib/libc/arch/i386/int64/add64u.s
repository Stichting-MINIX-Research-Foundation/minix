!	add64u() - unsigned to 64 bit addition		Author: Kees J. Bot
!								7 Dec 1995
.sect .text
.define _add64u, _add64ul

_add64u:			! u64_t add64u(u64_t i, unsigned j);
_add64ul:			! u64_t add64ul(u64_t i, unsigned long j);
	mov	eax, 4(esp)
	mov	edx, 8(esp)
	add	edx, 16(esp)
	mov	(eax), edx
	mov	edx, 12(esp)
	adc	edx, 0
	mov	4(eax), edx
	ret

!
! $PchId: add64u.ack.s,v 1.2 1996/04/11 18:59:57 philip Exp $
