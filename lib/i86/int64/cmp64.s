!	cmp64*() - 64 bit compare			Author: Kees J. Bot
!								24 Dec 1995
.sect .text
.define _cmp64, _cmp64u, _cmp64ul

_cmp64:				! int cmp64(u64_t i, u64_t j);
	mov	bx, sp
cmp64:	xor	ax, ax
	mov	dx, 2(bx)
	sub	dx, 10(bx)
	mov	dx, 4(bx)
	sbb	dx, 12(bx)
	mov	dx, 6(bx)
	sbb	dx, 14(bx)
	mov	dx, 8(bx)
	sbb	dx, 16(bx)
	sbb	ax, ax			! ax = - (i < j)
	mov	dx, 10(bx)
	sub	dx, 2(bx)
	mov	dx, 12(bx)
	sbb	dx, 4(bx)
	mov	dx, 14(bx)
	sbb	dx, 6(bx)
	mov	dx, 16(bx)
	sbb	dx, 8(bx)
	adc	ax, #0			! ax = (i > j) - (i < j)
	ret

_cmp64u:			! int cmp64u(u64_t i, unsigned j);
	mov	bx, sp
	push	16(bx)
	mov	16(bx), #0
	push	14(bx)
	mov	14(bx), #0
	push	12(bx)
	mov	12(bx), #0
	call	cmp64
	pop	12(bx)
	pop	14(bx)
	pop	16(bx)
	ret

_cmp64ul:			! int cmp64ul(u64_t i, unsigned long j);
	mov	bx, sp
	push	14(bx)
	mov	14(bx), #0
	push	12(bx)
	mov	12(bx), #0
	call	cmp64
	pop	12(bx)
	pop	14(bx)
	ret
