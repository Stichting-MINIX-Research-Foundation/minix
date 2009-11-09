!	oneC_sum() - One complement`s checksum		Author: Kees J. Bot
!								23 May 1998
! See RFC 1071, "Computing the Internet checksum"
! See also the C version of this code.

.text

.define _oneC_sum
	.align	4
_oneC_sum:
	push	bp
	mov	bp, sp
	push	si
	push	di
	mov	ax, 4(bp)		! Checksum of previous block
	mov	si, 6(bp)		! Data to compute checksum over
	mov	di, 8(bp)		! Number of bytes

	xor	dx, dx
	xorb	cl, cl
align:	test	si, #1			! Is the data aligned?
	jz	aligned
	test	di, di
	jz	0f
	movb	dh, (si)		! First unaligned byte in high half of
	dec	di			! the dx register, i.e. rotate 8 bits
0:	inc	si
	movb	cl, #8			! Number of bits "rotated"
	ror	ax, cl			! Rotate the checksum likewise
aligned:add	ax, dx			! Summate the unaligned byte
	adc	ax, #0			! Add carry back in for one`s complement

	jmp	add6test
	.align	4
add6:	add	ax, (si)		! Six times unrolled loop, see below
	adc	ax, 2(si)
	adc	ax, 4(si)
	adc	ax, 6(si)
	adc	ax, 8(si)
	adc	ax, 10(si)
	adc	ax, #0
	add	si, #12
add6test:
	sub	di, #12
	jae	add6
	add	di, #12

	jmp	add1test
	.align	4
add1:	add	ax, (si)		! while ((di -= 2) >= 0)
	adc	ax, #0			!	ax += *si++;
	add	si, #2			! di += 2;
add1test:
	sub	di, #2
	jae	add1
	add	di, #2

	jz	done			! Is there an extra byte?
	movb	dl, (si)		! Load extra byte in word
	xorb	dh, dh
	add	ax, dx			! Add in the last bits
	adc	ax, #0
done:
	rol	ax, cl			! Undo the rotation at the beginning
	pop	di
	pop	si
	pop	bp
	ret
