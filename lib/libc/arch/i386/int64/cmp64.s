!	cmp64*() - 64 bit compare			Author: Kees J. Bot
!								7 Dec 1995
.sect .text
.define _cmp64, _cmp64u, _cmp64ul

_cmp64:				! int cmp64(u64_t i, u64_t j);
	mov	ecx, esp
cmp64:	xor	eax, eax
	mov	edx, 4(ecx)
	sub	edx, 12(ecx)
	mov	edx, 8(ecx)
	sbb	edx, 16(ecx)
	sbb	eax, eax		! eax = - (i < j)
	mov	edx, 12(ecx)
	sub	edx, 4(ecx)
	mov	edx, 16(ecx)
	sbb	edx, 8(ecx)
	adc	eax, 0			! eax = (i > j) - (i < j)
	ret

_cmp64u:			! int cmp64u(u64_t i, unsigned j);
_cmp64ul:			! int cmp64ul(u64_t i, unsigned long j);
	mov	ecx, esp
	push	16(ecx)
	mov	16(ecx), 0
	call	cmp64
	pop	16(ecx)
	ret

!
! $PchId: cmp64.ack.s,v 1.2 1996/04/11 18:59:57 philip Exp $
