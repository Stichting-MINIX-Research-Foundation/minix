!	cvu64() - unsigned converted to 64 bit		Author: Kees J. Bot
!								7 Dec 1995
.sect .text
.define _cvu64, _cvul64

_cvu64:				! u64_t cvu64(unsigned i);
_cvul64:			! u64_t cvul64(unsigned long i);
	mov	eax, 4(esp)
	mov	edx, 8(esp)
	mov	(eax), edx
	mov	4(eax), 0
	ret

!
! $PchId: cvu64.ack.s,v 1.2 1996/04/11 18:59:57 philip Exp $
