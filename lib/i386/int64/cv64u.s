!	cv64u() - 64 bit converted to unsigned		Author: Kees J. Bot
!								7 Dec 1995
.sect .text
.define _cv64u, _cv64ul

_cv64u:				! unsigned cv64u(u64_t i);
_cv64ul:			! unsigned long cv64ul(u64_t i);
	mov	eax, 4(esp)
	cmp	8(esp), 0		! return ULONG_MAX if really big
	jz	0f
	mov	eax, -1
0:	ret

!
! $PchId: cv64u.ack.s,v 1.2 1996/04/11 18:59:57 philip Exp $
