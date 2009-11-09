!	_memmove()					Author: Kees J. Bot
!								27 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! void *_memmove(void *s1, const void *s2, size_t n)
!	Copy a chunk of memory.  Handle overlap.
!
.sect .text
.define __memmove, __memcpy
__memmove:
	push	bp
	mov	bp, sp
	push	si
	push	di
	mov	di, 4(bp)	! String s1
	mov	si, 6(bp)	! String s2
	mov	cx, 8(bp)	! Length
	mov	ax, di
	sub	ax, si
	cmp	ax, cx
	jb	downwards	! if (s2 - s1) < n then copy downwards
__memcpy:
	cld			! Clear direction bit: upwards
	cmp	cx, #16
	jb	upbyte		! Don't bother being smart with short arrays
	mov	ax, si
	or	ax, di
	testb	al, #1
	jnz	upbyte		! Bit 0 set, use byte copy
upword:	shr	cx, #1
    rep	movs			! Copy words
	adc	cx, cx		! One more byte?
upbyte:
    rep	movsb			! Copy bytes
done:	mov	ax, 4(bp)	! Absolutely noone cares about this value
	pop	di
	pop	si
	pop	bp
	ret

! Handle bad overlap by copying downwards, don't bother to do word copies.
downwards:
	std			! Set direction bit: downwards
	add	si, cx
	dec	si
	add	di, cx
	dec	di
    rep	movsb			! Copy bytes
	cld
	jmp	done
