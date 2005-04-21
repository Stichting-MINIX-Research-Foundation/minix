!	add64() - 64 bit addition			Author: Kees J. Bot
!								7 Dec 1995
.sect .text
.define _add64

_add64:				! u64_t add64(u64_t i, u64_t j);
	mov	eax, 4(esp)
	mov	edx, 8(esp)
	add	edx, 16(esp)
	mov	(eax), edx
	mov	edx, 12(esp)
	adc	edx, 20(esp)
	mov	4(eax), edx
	ret

!
! $PchId: add64.ack.s,v 1.2 1996/04/11 18:59:57 philip Exp $
