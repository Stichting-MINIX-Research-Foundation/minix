!	oneC_sum() - One complement`s checksum		Author: Kees J. Bot
!								9 May 1995
! See RFC 1071, "Computing the Internet checksum"
! See also the C version of this code.

.sect .text

.define _oneC_sum
	.align	16
_oneC_sum:
	push	ebp
	mov	ebp, esp
	push	esi
	push	edi
	movzx	eax, 8(ebp)		! Checksum of previous block
	mov	esi, 12(ebp)		! Data to compute checksum over
	mov	edi, 16(ebp)		! Number of bytes

	xor	edx, edx
	xorb	cl, cl
align:	test	esi, 3			! Is the data aligned?
	jz	aligned
	test	edi, edi
	jz	0f
	movb	dl, (esi)		! Rotate the first unaligned bytes
	dec	edi			! into the edx register
0:	inc	esi
	ror	edx, 8
	ror	eax, 8			! Rotate the checksum likewise
	addb	cl, 8			! Number of bits rotated
	jmp	align
aligned:add	eax, edx		! Summate the unaligned bytes
	adc	eax, 0			! Add carry back in for one`s complement

	jmp	add6test
	.align	16
add6:	add	eax, (esi)		! Six times unrolled loop, see below
	adc	eax, 4(esi)
	adc	eax, 8(esi)
	adc	eax, 12(esi)
	adc	eax, 16(esi)
	adc	eax, 20(esi)
	adc	eax, 0
	add	esi, 24
add6test:
	sub	edi, 24
	jae	add6
	add	edi, 24

	jmp	add1test
	.align	16
add1:	add	eax, (esi)		! while ((edi -= 4) >= 0)
	adc	eax, 0			!	eax += *esi++;
	add	esi, 4			! edi += 4;
add1test:
	sub	edi, 4
	jae	add1
	add	edi, 4

	jz	done			! Are there extra bytes?
	mov	edx, (esi)		! Load extra bytes in a full dword
	and	edx, mask-4(edi*4)	! Mask off excess
	add	eax, edx		! Add in the last bits
	adc	eax, 0
done:	rol	eax, cl			! Undo the rotation at the beginning
	mov	edx, eax
	shr	eax, 16
  o16	add	ax, dx			! Add the two words in eax to form
  o16	adc	ax, 0			! a 16 bit sum
	pop	edi
	pop	esi
	pop	ebp
	ret

.sect .rom
	.align	4
mask:	.data4	0x000000FF, 0x0000FFFF, 0x00FFFFFF

!
! $PchId: oneC_sum.ack.s,v 1.2 1996/03/12 19:33:51 philip Exp $
