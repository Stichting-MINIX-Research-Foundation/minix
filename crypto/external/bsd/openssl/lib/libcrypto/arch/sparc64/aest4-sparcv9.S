.register	%g2,#scratch
.register	%g3,#scratch

.text

.globl	aes_t4_encrypt
.align	32
aes_t4_encrypt:
	andcc		%o0, 7, %g1		! is input aligned?
	andn		%o0, 7, %o0

	ldx		[%o2 + 0], %g4
	ldx		[%o2 + 8], %g5

	ldx		[%o0 + 0], %o4
	bz,pt		%icc, 1f
	ldx		[%o0 + 8], %o5
	ldx		[%o0 + 16], %o0
	sll		%g1, 3, %g1
	sub		%g0, %g1, %o3
	sllx		%o4, %g1, %o4
	sllx		%o5, %g1, %g1
	srlx		%o5, %o3, %o5
	srlx		%o0, %o3, %o3
	or		%o5, %o4, %o4
	or		%o3, %g1, %o5
1:
	ld		[%o2 + 240], %o3
	ldd		[%o2 + 16], %f12
	ldd		[%o2 + 24], %f14
	xor		%g4, %o4, %o4
	xor		%g5, %o5, %o5
	.word	0x81b0230c !movxtod	%o4,%f0
	.word	0x85b0230d !movxtod	%o5,%f2
	srl		%o3, 1, %o3
	ldd		[%o2 + 32], %f16
	sub		%o3, 1, %o3
	ldd		[%o2 + 40], %f18
	add		%o2, 48, %o2

.Lenc:
	.word	0x88cb0400 !aes_eround01	%f12,%f0,%f2,%f4
	.word	0x84cb8420 !aes_eround23	%f14,%f0,%f2,%f2
	ldd		[%o2 + 0], %f12
	ldd		[%o2 + 8], %f14
	sub		%o3,1,%o3
	.word	0x80cc0404 !aes_eround01	%f16,%f4,%f2,%f0
	.word	0x84cc8424 !aes_eround23	%f18,%f4,%f2,%f2
	ldd		[%o2 + 16], %f16
	ldd		[%o2 + 24], %f18
	brnz,pt		%o3, .Lenc
	add		%o2, 32, %o2

	andcc		%o1, 7, %o4		! is output aligned?
	.word	0x88cb0400 !aes_eround01	%f12,%f0,%f2,%f4
	.word	0x84cb8420 !aes_eround23	%f14,%f0,%f2,%f2
	.word	0x80cc0484 !aes_eround01_l	%f16,%f4,%f2,%f0
	.word	0x84cc84a4 !aes_eround23_l	%f18,%f4,%f2,%f2

	bnz,pn		%icc, 2f
	nop

	std		%f0, [%o1 + 0]
	retl
	std		%f2, [%o1 + 8]

2:	.word	0x93b24340 !alignaddrl	%o1,%g0,%o1
	mov		0xff, %o5
	srl		%o5, %o4, %o5

	.word	0x89b00900 !faligndata	%f0,%f0,%f4
	.word	0x8db00902 !faligndata	%f0,%f2,%f6
	.word	0x91b08902 !faligndata	%f2,%f2,%f8

	stda		%f4, [%o1 + %o5]0xc0	! partial store
	std		%f6, [%o1 + 8]
	add		%o1, 16, %o1
	orn		%g0, %o5, %o5
	retl
	stda		%f8, [%o1 + %o5]0xc0	! partial store
.type	aes_t4_encrypt,#function
.size	aes_t4_encrypt,.-aes_t4_encrypt

.globl	aes_t4_decrypt
.align	32
aes_t4_decrypt:
	andcc		%o0, 7, %g1		! is input aligned?
	andn		%o0, 7, %o0

	ldx		[%o2 + 0], %g4
	ldx		[%o2 + 8], %g5

	ldx		[%o0 + 0], %o4
	bz,pt		%icc, 1f
	ldx		[%o0 + 8], %o5
	ldx		[%o0 + 16], %o0
	sll		%g1, 3, %g1
	sub		%g0, %g1, %o3
	sllx		%o4, %g1, %o4
	sllx		%o5, %g1, %g1
	srlx		%o5, %o3, %o5
	srlx		%o0, %o3, %o3
	or		%o5, %o4, %o4
	or		%o3, %g1, %o5
1:
	ld		[%o2 + 240], %o3
	ldd		[%o2 + 16], %f12
	ldd		[%o2 + 24], %f14
	xor		%g4, %o4, %o4
	xor		%g5, %o5, %o5
	.word	0x81b0230c !movxtod	%o4,%f0
	.word	0x85b0230d !movxtod	%o5,%f2
	srl		%o3, 1, %o3
	ldd		[%o2 + 32], %f16
	sub		%o3, 1, %o3
	ldd		[%o2 + 40], %f18
	add		%o2, 48, %o2

.Ldec:
	.word	0x88cb0440 !aes_dround01	%f12,%f0,%f2,%f4
	.word	0x84cb8460 !aes_dround23	%f14,%f0,%f2,%f2
	ldd		[%o2 + 0], %f12
	ldd		[%o2 + 8], %f14
	sub		%o3,1,%o3
	.word	0x80cc0444 !aes_dround01	%f16,%f4,%f2,%f0
	.word	0x84cc8464 !aes_dround23	%f18,%f4,%f2,%f2
	ldd		[%o2 + 16], %f16
	ldd		[%o2 + 24], %f18
	brnz,pt		%o3, .Ldec
	add		%o2, 32, %o2

	andcc		%o1, 7, %o4		! is output aligned?
	.word	0x88cb0440 !aes_dround01	%f12,%f0,%f2,%f4
	.word	0x84cb8460 !aes_dround23	%f14,%f0,%f2,%f2
	.word	0x80cc04c4 !aes_dround01_l	%f16,%f4,%f2,%f0
	.word	0x84cc84e4 !aes_dround23_l	%f18,%f4,%f2,%f2

	bnz,pn		%icc, 2f
	nop

	std		%f0, [%o1 + 0]
	retl
	std		%f2, [%o1 + 8]

2:	.word	0x93b24340 !alignaddrl	%o1,%g0,%o1
	mov		0xff, %o5
	srl		%o5, %o4, %o5

	.word	0x89b00900 !faligndata	%f0,%f0,%f4
	.word	0x8db00902 !faligndata	%f0,%f2,%f6
	.word	0x91b08902 !faligndata	%f2,%f2,%f8

	stda		%f4, [%o1 + %o5]0xc0	! partial store
	std		%f6, [%o1 + 8]
	add		%o1, 16, %o1
	orn		%g0, %o5, %o5
	retl
	stda		%f8, [%o1 + %o5]0xc0	! partial store
.type	aes_t4_decrypt,#function
.size	aes_t4_decrypt,.-aes_t4_decrypt
.globl	aes_t4_set_encrypt_key
.align	32
aes_t4_set_encrypt_key:
.Lset_encrypt_key:
	and		%o0, 7, %o3
	.word	0x91b20300 !alignaddr	%o0,%g0,%o0
	cmp		%o1, 192
	ldd		[%o0 + 0], %f0
	bl,pt		%icc,.L128
	ldd		[%o0 + 8], %f2

	be,pt		%icc,.L192
	ldd		[%o0 + 16], %f4
	brz,pt		%o3, .L256aligned
	ldd		[%o0 + 24], %f6

	ldd		[%o0 + 32], %f8
	.word	0x81b00902 !faligndata	%f0,%f2,%f0
	.word	0x85b08904 !faligndata	%f2,%f4,%f2
	.word	0x89b10906 !faligndata	%f4,%f6,%f4
	.word	0x8db18908 !faligndata	%f6,%f8,%f6
.L256aligned:
	std		%f0, [%o2 + 0]
	.word	0x80c80106 !aes_kexpand1	%f0,%f6,0,%f0
	std		%f2, [%o2 + 8]
	.word	0x85b0a620 !aes_kexpand2	%f2,%f0,%f2
	std		%f4, [%o2 + 16]
	.word	0x89b12602 !aes_kexpand0	%f4,%f2,%f4
	std		%f6, [%o2 + 24]
	.word	0x8db1a624 !aes_kexpand2	%f6,%f4,%f6
	std		%f0, [%o2 + 32]
	.word	0x80c80306 !aes_kexpand1	%f0,%f6,1,%f0
	std		%f2, [%o2 + 40]
	.word	0x85b0a620 !aes_kexpand2	%f2,%f0,%f2
	std		%f4, [%o2 + 48]
	.word	0x89b12602 !aes_kexpand0	%f4,%f2,%f4
	std		%f6, [%o2 + 56]
	.word	0x8db1a624 !aes_kexpand2	%f6,%f4,%f6
	std		%f0, [%o2 + 64]
	.word	0x80c80506 !aes_kexpand1	%f0,%f6,2,%f0
	std		%f2, [%o2 + 72]
	.word	0x85b0a620 !aes_kexpand2	%f2,%f0,%f2
	std		%f4, [%o2 + 80]
	.word	0x89b12602 !aes_kexpand0	%f4,%f2,%f4
	std		%f6, [%o2 + 88]
	.word	0x8db1a624 !aes_kexpand2	%f6,%f4,%f6
	std		%f0, [%o2 + 96]
	.word	0x80c80706 !aes_kexpand1	%f0,%f6,3,%f0
	std		%f2, [%o2 + 104]
	.word	0x85b0a620 !aes_kexpand2	%f2,%f0,%f2
	std		%f4, [%o2 + 112]
	.word	0x89b12602 !aes_kexpand0	%f4,%f2,%f4
	std		%f6, [%o2 + 120]
	.word	0x8db1a624 !aes_kexpand2	%f6,%f4,%f6
	std		%f0, [%o2 + 128]
	.word	0x80c80906 !aes_kexpand1	%f0,%f6,4,%f0
	std		%f2, [%o2 + 136]
	.word	0x85b0a620 !aes_kexpand2	%f2,%f0,%f2
	std		%f4, [%o2 + 144]
	.word	0x89b12602 !aes_kexpand0	%f4,%f2,%f4
	std		%f6, [%o2 + 152]
	.word	0x8db1a624 !aes_kexpand2	%f6,%f4,%f6
	std		%f0, [%o2 + 160]
	.word	0x80c80b06 !aes_kexpand1	%f0,%f6,5,%f0
	std		%f2, [%o2 + 168]
	.word	0x85b0a620 !aes_kexpand2	%f2,%f0,%f2
	std		%f4, [%o2 + 176]
	.word	0x89b12602 !aes_kexpand0	%f4,%f2,%f4
	std		%f6, [%o2 + 184]
	.word	0x8db1a624 !aes_kexpand2	%f6,%f4,%f6
	std		%f0, [%o2 + 192]
	.word	0x80c80d06 !aes_kexpand1	%f0,%f6,6,%f0
	std		%f2, [%o2 + 200]
	.word	0x85b0a620 !aes_kexpand2	%f2,%f0,%f2
	std		%f4, [%o2 + 208]
	std		%f6, [%o2 + 216]
	std		%f0, [%o2 + 224]
	std		%f2, [%o2 + 232]

	mov		14, %o3
	st		%o3, [%o2 + 240]
	retl
	xor		%o0, %o0, %o0

.align	16
.L192:
	brz,pt		%o3, .L192aligned
	nop

	ldd		[%o0 + 24], %f6
	.word	0x81b00902 !faligndata	%f0,%f2,%f0
	.word	0x85b08904 !faligndata	%f2,%f4,%f2
	.word	0x89b10906 !faligndata	%f4,%f6,%f4
.L192aligned:
	std		%f0, [%o2 + 0]
	.word	0x80c80104 !aes_kexpand1	%f0,%f4,0,%f0
	std		%f2, [%o2 + 8]
	.word	0x85b0a620 !aes_kexpand2	%f2,%f0,%f2
	std		%f4, [%o2 + 16]
	.word	0x89b12622 !aes_kexpand2	%f4,%f2,%f4
	std		%f0, [%o2 + 24]
	.word	0x80c80304 !aes_kexpand1	%f0,%f4,1,%f0
	std		%f2, [%o2 + 32]
	.word	0x85b0a620 !aes_kexpand2	%f2,%f0,%f2
	std		%f4, [%o2 + 40]
	.word	0x89b12622 !aes_kexpand2	%f4,%f2,%f4
	std		%f0, [%o2 + 48]
	.word	0x80c80504 !aes_kexpand1	%f0,%f4,2,%f0
	std		%f2, [%o2 + 56]
	.word	0x85b0a620 !aes_kexpand2	%f2,%f0,%f2
	std		%f4, [%o2 + 64]
	.word	0x89b12622 !aes_kexpand2	%f4,%f2,%f4
	std		%f0, [%o2 + 72]
	.word	0x80c80704 !aes_kexpand1	%f0,%f4,3,%f0
	std		%f2, [%o2 + 80]
	.word	0x85b0a620 !aes_kexpand2	%f2,%f0,%f2
	std		%f4, [%o2 + 88]
	.word	0x89b12622 !aes_kexpand2	%f4,%f2,%f4
	std		%f0, [%o2 + 96]
	.word	0x80c80904 !aes_kexpand1	%f0,%f4,4,%f0
	std		%f2, [%o2 + 104]
	.word	0x85b0a620 !aes_kexpand2	%f2,%f0,%f2
	std		%f4, [%o2 + 112]
	.word	0x89b12622 !aes_kexpand2	%f4,%f2,%f4
	std		%f0, [%o2 + 120]
	.word	0x80c80b04 !aes_kexpand1	%f0,%f4,5,%f0
	std		%f2, [%o2 + 128]
	.word	0x85b0a620 !aes_kexpand2	%f2,%f0,%f2
	std		%f4, [%o2 + 136]
	.word	0x89b12622 !aes_kexpand2	%f4,%f2,%f4
	std		%f0, [%o2 + 144]
	.word	0x80c80d04 !aes_kexpand1	%f0,%f4,6,%f0
	std		%f2, [%o2 + 152]
	.word	0x85b0a620 !aes_kexpand2	%f2,%f0,%f2
	std		%f4, [%o2 + 160]
	.word	0x89b12622 !aes_kexpand2	%f4,%f2,%f4
	std		%f0, [%o2 + 168]
	.word	0x80c80f04 !aes_kexpand1	%f0,%f4,7,%f0
	std		%f2, [%o2 + 176]
	.word	0x85b0a620 !aes_kexpand2	%f2,%f0,%f2
	std		%f4, [%o2 + 184]
	std		%f0, [%o2 + 192]
	std		%f2, [%o2 + 200]

	mov		12, %o3
	st		%o3, [%o2 + 240]
	retl
	xor		%o0, %o0, %o0

.align	16
.L128:
	brz,pt		%o3, .L128aligned
	nop

	ldd		[%o0 + 16], %f4
	.word	0x81b00902 !faligndata	%f0,%f2,%f0
	.word	0x85b08904 !faligndata	%f2,%f4,%f2
.L128aligned:
	std		%f0, [%o2 + 0]
	.word	0x80c80102 !aes_kexpand1	%f0,%f2,0,%f0
	std		%f2, [%o2 + 8]
	.word	0x85b0a620 !aes_kexpand2	%f2,%f0,%f2
	std		%f0, [%o2 + 16]
	.word	0x80c80302 !aes_kexpand1	%f0,%f2,1,%f0
	std		%f2, [%o2 + 24]
	.word	0x85b0a620 !aes_kexpand2	%f2,%f0,%f2
	std		%f0, [%o2 + 32]
	.word	0x80c80502 !aes_kexpand1	%f0,%f2,2,%f0
	std		%f2, [%o2 + 40]
	.word	0x85b0a620 !aes_kexpand2	%f2,%f0,%f2
	std		%f0, [%o2 + 48]
	.word	0x80c80702 !aes_kexpand1	%f0,%f2,3,%f0
	std		%f2, [%o2 + 56]
	.word	0x85b0a620 !aes_kexpand2	%f2,%f0,%f2
	std		%f0, [%o2 + 64]
	.word	0x80c80902 !aes_kexpand1	%f0,%f2,4,%f0
	std		%f2, [%o2 + 72]
	.word	0x85b0a620 !aes_kexpand2	%f2,%f0,%f2
	std		%f0, [%o2 + 80]
	.word	0x80c80b02 !aes_kexpand1	%f0,%f2,5,%f0
	std		%f2, [%o2 + 88]
	.word	0x85b0a620 !aes_kexpand2	%f2,%f0,%f2
	std		%f0, [%o2 + 96]
	.word	0x80c80d02 !aes_kexpand1	%f0,%f2,6,%f0
	std		%f2, [%o2 + 104]
	.word	0x85b0a620 !aes_kexpand2	%f2,%f0,%f2
	std		%f0, [%o2 + 112]
	.word	0x80c80f02 !aes_kexpand1	%f0,%f2,7,%f0
	std		%f2, [%o2 + 120]
	.word	0x85b0a620 !aes_kexpand2	%f2,%f0,%f2
	std		%f0, [%o2 + 128]
	.word	0x80c81102 !aes_kexpand1	%f0,%f2,8,%f0
	std		%f2, [%o2 + 136]
	.word	0x85b0a620 !aes_kexpand2	%f2,%f0,%f2
	std		%f0, [%o2 + 144]
	.word	0x80c81302 !aes_kexpand1	%f0,%f2,9,%f0
	std		%f2, [%o2 + 152]
	.word	0x85b0a620 !aes_kexpand2	%f2,%f0,%f2
	std		%f0, [%o2 + 160]
	std		%f2, [%o2 + 168]

	mov		10, %o3
	st		%o3, [%o2 + 240]
	retl
	xor		%o0, %o0, %o0
.type	aes_t4_set_encrypt_key,#function
.size	aes_t4_set_encrypt_key,.-aes_t4_set_encrypt_key

.globl	aes_t4_set_decrypt_key
.align	32
aes_t4_set_decrypt_key:
	mov		%o7, %o5
	call		.Lset_encrypt_key
	nop

	mov		%o5, %o7
	sll		%o3, 4, %o0		! %o3 is number of rounds
	add		%o3, 2, %o3
	add		%o2, %o0, %o0	! %o0=%o2+16*rounds
	srl		%o3, 2, %o3		! %o3=(rounds+2)/4

.Lkey_flip:
	ldd		[%o2 + 0],  %f0
	ldd		[%o2 + 8],  %f2
	ldd		[%o2 + 16], %f4
	ldd		[%o2 + 24], %f6
	ldd		[%o0 + 0],  %f8
	ldd		[%o0 + 8],  %f10
	ldd		[%o0 - 16], %f12
	ldd		[%o0 - 8],  %f14
	sub		%o3, 1, %o3
	std		%f0, [%o0 + 0]
	std		%f2, [%o0 + 8]
	std		%f4, [%o0 - 16]
	std		%f6, [%o0 - 8]
	std		%f8, [%o2 + 0]
	std		%f10, [%o2 + 8]
	std		%f12, [%o2 + 16]
	std		%f14, [%o2 + 24]
	add		%o2, 32, %o2
	brnz		%o3, .Lkey_flip
	sub		%o0, 32, %o0

	retl
	xor		%o0, %o0, %o0
.type	aes_t4_set_decrypt_key,#function
.size	aes_t4_set_decrypt_key,.-aes_t4_set_decrypt_key
.align	32
_aes128_encrypt_1x:
	.word	0x88cc0400 !aes_eround01	%f16,%f0,%f2,%f4
	.word	0x84cc8420 !aes_eround23	%f18,%f0,%f2,%f2
	.word	0x80cd0404 !aes_eround01	%f20,%f4,%f2,%f0
	.word	0x84cd8424 !aes_eround23	%f22,%f4,%f2,%f2
	.word	0x88ce0400 !aes_eround01	%f24,%f0,%f2,%f4
	.word	0x84ce8420 !aes_eround23	%f26,%f0,%f2,%f2
	.word	0x80cf0404 !aes_eround01	%f28,%f4,%f2,%f0
	.word	0x84cf8424 !aes_eround23	%f30,%f4,%f2,%f2
	.word	0x88c84400 !aes_eround01	%f32,%f0,%f2,%f4
	.word	0x84c8c420 !aes_eround23	%f34,%f0,%f2,%f2
	.word	0x80c94404 !aes_eround01	%f36,%f4,%f2,%f0
	.word	0x84c9c424 !aes_eround23	%f38,%f4,%f2,%f2
	.word	0x88ca4400 !aes_eround01	%f40,%f0,%f2,%f4
	.word	0x84cac420 !aes_eround23	%f42,%f0,%f2,%f2
	.word	0x80cb4404 !aes_eround01	%f44,%f4,%f2,%f0
	.word	0x84cbc424 !aes_eround23	%f46,%f4,%f2,%f2
	.word	0x88cc4400 !aes_eround01	%f48,%f0,%f2,%f4
	.word	0x84ccc420 !aes_eround23	%f50,%f0,%f2,%f2
	.word	0x80cd4484 !aes_eround01_l	%f52,%f4,%f2,%f0
	retl
	.word	0x84cdc4a4 !aes_eround23_l	%f54,%f4,%f2,%f2
.type	_aes128_encrypt_1x,#function
.size	_aes128_encrypt_1x,.-_aes128_encrypt_1x

.align	32
_aes128_encrypt_2x:
	.word	0x90cc0400 !aes_eround01	%f16,%f0,%f2,%f8
	.word	0x84cc8420 !aes_eround23	%f18,%f0,%f2,%f2
	.word	0x94cc0c04 !aes_eround01	%f16,%f4,%f6,%f10
	.word	0x8ccc8c24 !aes_eround23	%f18,%f4,%f6,%f6
	.word	0x80cd0408 !aes_eround01	%f20,%f8,%f2,%f0
	.word	0x84cd8428 !aes_eround23	%f22,%f8,%f2,%f2
	.word	0x88cd0c0a !aes_eround01	%f20,%f10,%f6,%f4
	.word	0x8ccd8c2a !aes_eround23	%f22,%f10,%f6,%f6
	.word	0x90ce0400 !aes_eround01	%f24,%f0,%f2,%f8
	.word	0x84ce8420 !aes_eround23	%f26,%f0,%f2,%f2
	.word	0x94ce0c04 !aes_eround01	%f24,%f4,%f6,%f10
	.word	0x8cce8c24 !aes_eround23	%f26,%f4,%f6,%f6
	.word	0x80cf0408 !aes_eround01	%f28,%f8,%f2,%f0
	.word	0x84cf8428 !aes_eround23	%f30,%f8,%f2,%f2
	.word	0x88cf0c0a !aes_eround01	%f28,%f10,%f6,%f4
	.word	0x8ccf8c2a !aes_eround23	%f30,%f10,%f6,%f6
	.word	0x90c84400 !aes_eround01	%f32,%f0,%f2,%f8
	.word	0x84c8c420 !aes_eround23	%f34,%f0,%f2,%f2
	.word	0x94c84c04 !aes_eround01	%f32,%f4,%f6,%f10
	.word	0x8cc8cc24 !aes_eround23	%f34,%f4,%f6,%f6
	.word	0x80c94408 !aes_eround01	%f36,%f8,%f2,%f0
	.word	0x84c9c428 !aes_eround23	%f38,%f8,%f2,%f2
	.word	0x88c94c0a !aes_eround01	%f36,%f10,%f6,%f4
	.word	0x8cc9cc2a !aes_eround23	%f38,%f10,%f6,%f6
	.word	0x90ca4400 !aes_eround01	%f40,%f0,%f2,%f8
	.word	0x84cac420 !aes_eround23	%f42,%f0,%f2,%f2
	.word	0x94ca4c04 !aes_eround01	%f40,%f4,%f6,%f10
	.word	0x8ccacc24 !aes_eround23	%f42,%f4,%f6,%f6
	.word	0x80cb4408 !aes_eround01	%f44,%f8,%f2,%f0
	.word	0x84cbc428 !aes_eround23	%f46,%f8,%f2,%f2
	.word	0x88cb4c0a !aes_eround01	%f44,%f10,%f6,%f4
	.word	0x8ccbcc2a !aes_eround23	%f46,%f10,%f6,%f6
	.word	0x90cc4400 !aes_eround01	%f48,%f0,%f2,%f8
	.word	0x84ccc420 !aes_eround23	%f50,%f0,%f2,%f2
	.word	0x94cc4c04 !aes_eround01	%f48,%f4,%f6,%f10
	.word	0x8ccccc24 !aes_eround23	%f50,%f4,%f6,%f6
	.word	0x80cd4488 !aes_eround01_l	%f52,%f8,%f2,%f0
	.word	0x84cdc4a8 !aes_eround23_l	%f54,%f8,%f2,%f2
	.word	0x88cd4c8a !aes_eround01_l	%f52,%f10,%f6,%f4
	retl
	.word	0x8ccdccaa !aes_eround23_l	%f54,%f10,%f6,%f6
.type	_aes128_encrypt_2x,#function
.size	_aes128_encrypt_2x,.-_aes128_encrypt_2x

.align	32
_aes128_loadkey:
	ldx		[%i3 + 0], %g4
	ldx		[%i3 + 8], %g5
	ldd		[%i3 + 16], %f16
	ldd		[%i3 + 24], %f18
	ldd		[%i3 + 32], %f20
	ldd		[%i3 + 40], %f22
	ldd		[%i3 + 48], %f24
	ldd		[%i3 + 56], %f26
	ldd		[%i3 + 64], %f28
	ldd		[%i3 + 72], %f30
	ldd		[%i3 + 80], %f32
	ldd		[%i3 + 88], %f34
	ldd		[%i3 + 96], %f36
	ldd		[%i3 + 104], %f38
	ldd		[%i3 + 112], %f40
	ldd		[%i3 + 120], %f42
	ldd		[%i3 + 128], %f44
	ldd		[%i3 + 136], %f46
	ldd		[%i3 + 144], %f48
	ldd		[%i3 + 152], %f50
	ldd		[%i3 + 160], %f52
	ldd		[%i3 + 168], %f54
	retl
	nop
.type	_aes128_loadkey,#function
.size	_aes128_loadkey,.-_aes128_loadkey
_aes128_load_enckey=_aes128_loadkey
_aes128_load_deckey=_aes128_loadkey

.globl	aes128_t4_cbc_encrypt
.align	32
aes128_t4_cbc_encrypt:
	save		%sp, -192, %sp
	cmp		%i2, 0
	be,pn		%xcc, .L128_cbc_enc_abort
	srln		%i2, 0, %i2		! needed on v8+, "nop" on v9
	sub		%i0, %i1, %l5	! %i0!=%i1
	ld		[%i4 + 0], %f0
	ld		[%i4 + 4], %f1
	ld		[%i4 + 8], %f2
	ld		[%i4 + 12], %f3
	prefetch	[%i0], 20
	prefetch	[%i0 + 63], 20
	call		_aes128_load_enckey
	and		%i0, 7, %l0
	andn		%i0, 7, %i0
	sll		%l0, 3, %l0
	mov		64, %l1
	mov		0xff, %l3
	sub		%l1, %l0, %l1
	and		%i1, 7, %l2
	cmp		%i2, 127
	movrnz		%l2, 0, %l5		! if (	%i1&7 ||
	movleu		%xcc, 0, %l5	!	%i2<128 ||
	brnz,pn		%l5, .L128cbc_enc_blk	!	%i0==%i1)
	srl		%l3, %l2, %l3

	.word	0xb3b64340 !alignaddrl	%i1,%g0,%i1
	srlx		%i2, 4, %i2
	prefetch	[%i1], 22

.L128_cbc_enc_loop:
	ldx		[%i0 + 0], %o0
	brz,pt		%l0, 4f
	ldx		[%i0 + 8], %o1

	ldx		[%i0 + 16], %o2
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	sllx		%o1, %l0, %o1
	or		%g1, %o0, %o0
	srlx		%o2, %l1, %o2
	or		%o2, %o1, %o1
4:
	xor		%g4, %o0, %o0		! ^= rk[0]
	xor		%g5, %o1, %o1
	.word	0x99b02308 !movxtod	%o0,%f12
	.word	0x9db02309 !movxtod	%o1,%f14

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= ivec
	.word	0x85b38d82 !fxor	%f14,%f2,%f2
	prefetch	[%i1 + 63], 22
	prefetch	[%i0 + 16+63], 20
	call		_aes128_encrypt_1x
	add		%i0, 16, %i0

	brnz,pn		%l2, 2f
	sub		%i2, 1, %i2
		
	std		%f0, [%i1 + 0]
	std		%f2, [%i1 + 8]
	brnz,pt		%i2, .L128_cbc_enc_loop
	add		%i1, 16, %i1
	st		%f0, [%i4 + 0]
	st		%f1, [%i4 + 4]
	st		%f2, [%i4 + 8]
	st		%f3, [%i4 + 12]
.L128_cbc_enc_abort:
	ret
	restore

.align	16
2:	ldxa		[%i0]0x82, %o0		! avoid read-after-write hazard
						! and ~3x deterioration
						! in inp==out case
	.word	0x89b00900 !faligndata	%f0,%f0,%f4		! handle unaligned output
	.word	0x8db00902 !faligndata	%f0,%f2,%f6
	.word	0x91b08902 !faligndata	%f2,%f2,%f8

	stda		%f4, [%i1 + %l3]0xc0	! partial store
	std		%f6, [%i1 + 8]
	add		%i1, 16, %i1
	orn		%g0, %l3, %l3
	stda		%f8, [%i1 + %l3]0xc0	! partial store

	brnz,pt		%i2, .L128_cbc_enc_loop+4
	orn		%g0, %l3, %l3
	st		%f0, [%i4 + 0]
	st		%f1, [%i4 + 4]
	st		%f2, [%i4 + 8]
	st		%f3, [%i4 + 12]
	ret
	restore

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
.align	32
.L128cbc_enc_blk:
	add	%i1, %i2, %l5
	and	%l5, 63, %l5	! tail
	sub	%i2, %l5, %i2
	add	%l5, 15, %l5	! round up to 16n
	srlx	%i2, 4, %i2
	srl	%l5, 4, %l5

.L128_cbc_enc_blk_loop:
	ldx		[%i0 + 0], %o0
	brz,pt		%l0, 5f
	ldx		[%i0 + 8], %o1

	ldx		[%i0 + 16], %o2
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	sllx		%o1, %l0, %o1
	or		%g1, %o0, %o0
	srlx		%o2, %l1, %o2
	or		%o2, %o1, %o1
5:
	xor		%g4, %o0, %o0		! ^= rk[0]
	xor		%g5, %o1, %o1
	.word	0x99b02308 !movxtod	%o0,%f12
	.word	0x9db02309 !movxtod	%o1,%f14

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= ivec
	.word	0x85b38d82 !fxor	%f14,%f2,%f2
	prefetch	[%i0 + 16+63], 20
	call		_aes128_encrypt_1x
	add		%i0, 16, %i0
	sub		%i2, 1, %i2
		
	stda		%f0, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	add		%i1, 8, %i1
	stda		%f2, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	brnz,pt		%i2, .L128_cbc_enc_blk_loop
	add		%i1, 8, %i1

	membar		#StoreLoad|#StoreStore
	brnz,pt		%l5, .L128_cbc_enc_loop
	mov		%l5, %i2
	st		%f0, [%i4 + 0]
	st		%f1, [%i4 + 4]
	st		%f2, [%i4 + 8]
	st		%f3, [%i4 + 12]
	ret
	restore
.type	aes128_t4_cbc_encrypt,#function
.size	aes128_t4_cbc_encrypt,.-aes128_t4_cbc_encrypt
.globl	aes128_t4_ctr32_encrypt
.align	32
aes128_t4_ctr32_encrypt:
	save		%sp, -192, %sp
	srln		%i2, 0, %i2		! needed on v8+, "nop" on v9

	prefetch	[%i0], 20
	prefetch	[%i0 + 63], 20
	call		_aes128_load_enckey
	sllx		%i2, 4, %i2

	ld		[%i4 + 0], %l4	! counter
	ld		[%i4 + 4], %l5
	ld		[%i4 + 8], %l6
	ld		[%i4 + 12], %l7

	sllx		%l4, 32, %o5
	or		%l5, %o5, %o5
	sllx		%l6, 32, %g1
	xor		%o5, %g4, %g4		! ^= rk[0]
	xor		%g1, %g5, %g5
	.word	0x9db02304 !movxtod	%g4,%f14		! most significant 64 bits

	sub		%i0, %i1, %l5	! %i0!=%i1
	and		%i0, 7, %l0
	andn		%i0, 7, %i0
	sll		%l0, 3, %l0
	mov		64, %l1
	mov		0xff, %l3
	sub		%l1, %l0, %l1
	and		%i1, 7, %l2
	cmp		%i2, 255
	movrnz		%l2, 0, %l5		! if (	%i1&7 ||
	movleu		%xcc, 0, %l5	!	%i2<256 ||
	brnz,pn		%l5, .L128_ctr32_blk	!	%i0==%i1)
	srl		%l3, %l2, %l3

	andcc		%i2, 16, %g0		! is number of blocks even?
	.word	0xb3b64340 !alignaddrl	%i1,%g0,%i1
	bz		%icc, .L128_ctr32_loop2x
	srlx		%i2, 4, %i2
.L128_ctr32_loop:
	ldx		[%i0 + 0], %o0
	brz,pt		%l0, 4f
	ldx		[%i0 + 8], %o1

	ldx		[%i0 + 16], %o2
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	sllx		%o1, %l0, %o1
	or		%g1, %o0, %o0
	srlx		%o2, %l1, %o2
	or		%o2, %o1, %o1
4:
	xor		%g5, %l7, %g1		! ^= rk[0]
	add		%l7, 1, %l7
	.word	0x85b02301 !movxtod	%g1,%f2
	srl		%l7, 0, %l7		! clruw
	prefetch	[%i1 + 63], 22
	prefetch	[%i0 + 16+63], 20
	.word	0x88cc040e !aes_eround01	%f16,%f14,%f2,%f4
	.word	0x84cc842e !aes_eround23	%f18,%f14,%f2,%f2
	call		_aes128_encrypt_1x+8
	add		%i0, 16, %i0

	.word	0x95b02308 !movxtod	%o0,%f10
	.word	0x99b02309 !movxtod	%o1,%f12
	.word	0x81b28d80 !fxor	%f10,%f0,%f0		! ^= inp
	.word	0x85b30d82 !fxor	%f12,%f2,%f2

	brnz,pn		%l2, 2f
	sub		%i2, 1, %i2
		
	std		%f0, [%i1 + 0]
	std		%f2, [%i1 + 8]
	brnz,pt		%i2, .L128_ctr32_loop2x
	add		%i1, 16, %i1

	ret
	restore

.align	16
2:	ldxa		[%i0]0x82, %o0		! avoid read-after-write hazard
						! and ~3x deterioration
						! in inp==out case
	.word	0x89b00900 !faligndata	%f0,%f0,%f4		! handle unaligned output
	.word	0x8db00902 !faligndata	%f0,%f2,%f6
	.word	0x91b08902 !faligndata	%f2,%f2,%f8
	stda		%f4, [%i1 + %l3]0xc0	! partial store
	std		%f6, [%i1 + 8]
	add		%i1, 16, %i1
	orn		%g0, %l3, %l3
	stda		%f8, [%i1 + %l3]0xc0	! partial store

	brnz,pt		%i2, .L128_ctr32_loop2x+4
	orn		%g0, %l3, %l3

	ret
	restore

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
.align	32
.L128_ctr32_loop2x:
	ldx		[%i0 + 0], %o0
	ldx		[%i0 + 8], %o1
	ldx		[%i0 + 16], %o2
	brz,pt		%l0, 4f
	ldx		[%i0 + 24], %o3

	ldx		[%i0 + 32], %o4
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	or		%g1, %o0, %o0
	sllx		%o1, %l0, %o1
	srlx		%o2, %l1, %g1
	or		%g1, %o1, %o1
	sllx		%o2, %l0, %o2
	srlx		%o3, %l1, %g1
	or		%g1, %o2, %o2
	sllx		%o3, %l0, %o3
	srlx		%o4, %l1, %o4
	or		%o4, %o3, %o3
4:
	xor		%g5, %l7, %g1		! ^= rk[0]
	add		%l7, 1, %l7
	.word	0x85b02301 !movxtod	%g1,%f2
	srl		%l7, 0, %l7		! clruw
	xor		%g5, %l7, %g1
	add		%l7, 1, %l7
	.word	0x8db02301 !movxtod	%g1,%f6
	srl		%l7, 0, %l7		! clruw
	prefetch	[%i1 + 63], 22
	prefetch	[%i0 + 32+63], 20
	.word	0x90cc040e !aes_eround01	%f16,%f14,%f2,%f8
	.word	0x84cc842e !aes_eround23	%f18,%f14,%f2,%f2
	.word	0x94cc0c0e !aes_eround01	%f16,%f14,%f6,%f10
	.word	0x8ccc8c2e !aes_eround23	%f18,%f14,%f6,%f6
	call		_aes128_encrypt_2x+16
	add		%i0, 32, %i0

	.word	0x91b02308 !movxtod	%o0,%f8
	.word	0x95b02309 !movxtod	%o1,%f10
	.word	0x99b0230a !movxtod	%o2,%f12
	.word	0x81b20d80 !fxor	%f8,%f0,%f0		! ^= inp
	.word	0x91b0230b !movxtod	%o3,%f8
	.word	0x85b28d82 !fxor	%f10,%f2,%f2
	.word	0x89b30d84 !fxor	%f12,%f4,%f4
	.word	0x8db20d86 !fxor	%f8,%f6,%f6

	brnz,pn		%l2, 2f
	sub		%i2, 2, %i2
		
	std		%f0, [%i1 + 0]
	std		%f2, [%i1 + 8]
	std		%f4, [%i1 + 16]
	std		%f6, [%i1 + 24]
	brnz,pt		%i2, .L128_ctr32_loop2x
	add		%i1, 32, %i1

	ret
	restore

.align	16
2:	ldxa		[%i0]0x82, %o0		! avoid read-after-write hazard
						! and ~3x deterioration
						! in inp==out case
	.word	0x91b00900 !faligndata	%f0,%f0,%f8		! handle unaligned output
	.word	0x81b00902 !faligndata	%f0,%f2,%f0
	.word	0x85b08904 !faligndata	%f2,%f4,%f2
	.word	0x89b10906 !faligndata	%f4,%f6,%f4
	.word	0x8db18906 !faligndata	%f6,%f6,%f6

	stda		%f8, [%i1 + %l3]0xc0	! partial store
	std		%f0, [%i1 + 8]
	std		%f2, [%i1 + 16]
	std		%f4, [%i1 + 24]
	add		%i1, 32, %i1
	orn		%g0, %l3, %l3
	stda		%f6, [%i1 + %l3]0xc0	! partial store

	brnz,pt		%i2, .L128_ctr32_loop2x+4
	orn		%g0, %l3, %l3

	ret
	restore

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
.align	32
.L128_ctr32_blk:
	add	%i1, %i2, %l5
	and	%l5, 63, %l5	! tail
	sub	%i2, %l5, %i2
	add	%l5, 15, %l5	! round up to 16n
	srlx	%i2, 4, %i2
	srl	%l5, 4, %l5
	sub	%i2, 1, %i2
	add	%l5, 1, %l5

.L128_ctr32_blk_loop2x:
	ldx		[%i0 + 0], %o0
	ldx		[%i0 + 8], %o1
	ldx		[%i0 + 16], %o2
	brz,pt		%l0, 5f
	ldx		[%i0 + 24], %o3

	ldx		[%i0 + 32], %o4
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	or		%g1, %o0, %o0
	sllx		%o1, %l0, %o1
	srlx		%o2, %l1, %g1
	or		%g1, %o1, %o1
	sllx		%o2, %l0, %o2
	srlx		%o3, %l1, %g1
	or		%g1, %o2, %o2
	sllx		%o3, %l0, %o3
	srlx		%o4, %l1, %o4
	or		%o4, %o3, %o3
5:
	xor		%g5, %l7, %g1		! ^= rk[0]
	add		%l7, 1, %l7
	.word	0x85b02301 !movxtod	%g1,%f2
	srl		%l7, 0, %l7		! clruw
	xor		%g5, %l7, %g1
	add		%l7, 1, %l7
	.word	0x8db02301 !movxtod	%g1,%f6
	srl		%l7, 0, %l7		! clruw
	prefetch	[%i0 + 32+63], 20
	.word	0x90cc040e !aes_eround01	%f16,%f14,%f2,%f8
	.word	0x84cc842e !aes_eround23	%f18,%f14,%f2,%f2
	.word	0x94cc0c0e !aes_eround01	%f16,%f14,%f6,%f10
	.word	0x8ccc8c2e !aes_eround23	%f18,%f14,%f6,%f6
	call		_aes128_encrypt_2x+16
	add		%i0, 32, %i0
	subcc		%i2, 2, %i2

	.word	0x91b02308 !movxtod	%o0,%f8
	.word	0x95b02309 !movxtod	%o1,%f10
	.word	0x99b0230a !movxtod	%o2,%f12
	.word	0x81b20d80 !fxor	%f8,%f0,%f0		! ^= inp
	.word	0x91b0230b !movxtod	%o3,%f8
	.word	0x85b28d82 !fxor	%f10,%f2,%f2
	.word	0x89b30d84 !fxor	%f12,%f4,%f4
	.word	0x8db20d86 !fxor	%f8,%f6,%f6

	stda		%f0, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	add		%i1, 8, %i1
	stda		%f2, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	add		%i1, 8, %i1
	stda		%f4, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	add		%i1, 8, %i1
	stda		%f6, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	bgu,pt		%xcc, .L128_ctr32_blk_loop2x
	add		%i1, 8, %i1

	add		%l5, %i2, %i2
	andcc		%i2, 1, %g0		! is number of blocks even?
	membar		#StoreLoad|#StoreStore
	bnz,pt		%icc, .L128_ctr32_loop
	srl		%i2, 0, %i2
	brnz,pn		%i2, .L128_ctr32_loop2x
	nop

	ret
	restore
.type	aes128_t4_ctr32_encrypt,#function
.size	aes128_t4_ctr32_encrypt,.-aes128_t4_ctr32_encrypt
.globl	aes128_t4_xts_encrypt
.align	32
aes128_t4_xts_encrypt:
	save		%sp, -192-16, %sp
	srln		%i2, 0, %i2		! needed on v8+, "nop" on v9

	mov		%i5, %o0
	add		%fp, 2047-16, %o1
	call		aes_t4_encrypt
	mov		%i4, %o2

	add		%fp, 2047-16, %l7
	ldxa		[%l7]0x88, %g2
	add		%fp, 2047-8, %l7
	ldxa		[%l7]0x88, %g3		! %g3:%g2 is tweak

	sethi		%hi(0x76543210), %l7
	or		%l7, %lo(0x76543210), %l7
	.word	0x81b5c320 !bmask	%l7,%g0,%g0		! byte swap mask

	prefetch	[%i0], 20
	prefetch	[%i0 + 63], 20
	call		_aes128_load_enckey
	and		%i2, 15,  %i5
	and		%i2, -16, %i2

	sub		%i0, %i1, %l5	! %i0!=%i1
	and		%i0, 7, %l0
	andn		%i0, 7, %i0
	sll		%l0, 3, %l0
	mov		64, %l1
	mov		0xff, %l3
	sub		%l1, %l0, %l1
	and		%i1, 7, %l2
	cmp		%i2, 255
	movrnz		%l2, 0, %l5		! if (	%i1&7 ||
	movleu		%xcc, 0, %l5	!	%i2<256 ||
	brnz,pn		%l5, .L128_xts_enblk !	%i0==%i1)
	srl		%l3, %l2, %l3

	andcc		%i2, 16, %g0		! is number of blocks even?
	.word	0xb3b64340 !alignaddrl	%i1,%g0,%i1
	bz		%icc, .L128_xts_enloop2x
	srlx		%i2, 4, %i2
.L128_xts_enloop:
	ldx		[%i0 + 0], %o0
	brz,pt		%l0, 4f
	ldx		[%i0 + 8], %o1

	ldx		[%i0 + 16], %o2
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	sllx		%o1, %l0, %o1
	or		%g1, %o0, %o0
	srlx		%o2, %l1, %o2
	or		%o2, %o1, %o1
4:
	.word	0x99b02302 !movxtod	%g2,%f12
	.word	0x9db02303 !movxtod	%g3,%f14
	.word	0x99b3098c !bshuffle	%f12,%f12,%f12
	.word	0x9db3898e !bshuffle	%f14,%f14,%f14

	xor		%g4, %o0, %o0		! ^= rk[0]
	xor		%g5, %o1, %o1
	.word	0x81b02308 !movxtod	%o0,%f0
	.word	0x85b02309 !movxtod	%o1,%f2

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= tweak[0]
	.word	0x85b38d82 !fxor	%f14,%f2,%f2

	prefetch	[%i1 + 63], 22
	prefetch	[%i0 + 16+63], 20
	call		_aes128_encrypt_1x
	add		%i0, 16, %i0

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= tweak[0]
	.word	0x85b38d82 !fxor	%f14,%f2,%f2

	srax		%g3, 63, %l7		! next tweak value
	addcc		%g2, %g2, %g2
	and		%l7, 0x87, %l7
	.word	0x87b0c223 !addxc	%g3,%g3,%g3
	xor		%l7, %g2, %g2

	brnz,pn		%l2, 2f
	sub		%i2, 1, %i2
		
	std		%f0, [%i1 + 0]
	std		%f2, [%i1 + 8]
	brnz,pt		%i2, .L128_xts_enloop2x
	add		%i1, 16, %i1

	brnz,pn		%i5, .L128_xts_ensteal
	nop

	ret
	restore

.align	16
2:	ldxa		[%i0]0x82, %o0		! avoid read-after-write hazard
						! and ~3x deterioration
						! in inp==out case
	.word	0x89b00900 !faligndata	%f0,%f0,%f4		! handle unaligned output
	.word	0x8db00902 !faligndata	%f0,%f2,%f6
	.word	0x91b08902 !faligndata	%f2,%f2,%f8
	stda		%f4, [%i1 + %l3]0xc0	! partial store
	std		%f6, [%i1 + 8]
	add		%i1, 16, %i1
	orn		%g0, %l3, %l3
	stda		%f8, [%i1 + %l3]0xc0	! partial store

	brnz,pt		%i2, .L128_xts_enloop2x+4
	orn		%g0, %l3, %l3

	brnz,pn		%i5, .L128_xts_ensteal
	nop

	ret
	restore

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
.align	32
.L128_xts_enloop2x:
	ldx		[%i0 + 0], %o0
	ldx		[%i0 + 8], %o1
	ldx		[%i0 + 16], %o2
	brz,pt		%l0, 4f
	ldx		[%i0 + 24], %o3

	ldx		[%i0 + 32], %o4
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	or		%g1, %o0, %o0
	sllx		%o1, %l0, %o1
	srlx		%o2, %l1, %g1
	or		%g1, %o1, %o1
	sllx		%o2, %l0, %o2
	srlx		%o3, %l1, %g1
	or		%g1, %o2, %o2
	sllx		%o3, %l0, %o3
	srlx		%o4, %l1, %o4
	or		%o4, %o3, %o3
4:
	.word	0x99b02302 !movxtod	%g2,%f12
	.word	0x9db02303 !movxtod	%g3,%f14
	.word	0x99b3098c !bshuffle	%f12,%f12,%f12
	.word	0x9db3898e !bshuffle	%f14,%f14,%f14

	srax		%g3, 63, %l7		! next tweak value
	addcc		%g2, %g2, %g2
	and		%l7, 0x87, %l7
	.word	0x87b0c223 !addxc	%g3,%g3,%g3
	xor		%l7, %g2, %g2

	.word	0x91b02302 !movxtod	%g2,%f8
	.word	0x95b02303 !movxtod	%g3,%f10
	.word	0x91b20988 !bshuffle	%f8,%f8,%f8
	.word	0x95b2898a !bshuffle	%f10,%f10,%f10

	xor		%g4, %o0, %o0		! ^= rk[0]
	xor		%g5, %o1, %o1
	xor		%g4, %o2, %o2		! ^= rk[0]
	xor		%g5, %o3, %o3
	.word	0x81b02308 !movxtod	%o0,%f0
	.word	0x85b02309 !movxtod	%o1,%f2
	.word	0x89b0230a !movxtod	%o2,%f4
	.word	0x8db0230b !movxtod	%o3,%f6

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= tweak[0]
	.word	0x85b38d82 !fxor	%f14,%f2,%f2
	.word	0x89b20d84 !fxor	%f8,%f4,%f4		! ^= tweak[0]
	.word	0x8db28d86 !fxor	%f10,%f6,%f6

	prefetch	[%i1 + 63], 22
	prefetch	[%i0 + 32+63], 20
	call		_aes128_encrypt_2x
	add		%i0, 32, %i0

	.word	0x91b02302 !movxtod	%g2,%f8
	.word	0x95b02303 !movxtod	%g3,%f10

	srax		%g3, 63, %l7		! next tweak value
	addcc		%g2, %g2, %g2
	and		%l7, 0x87, %l7
	.word	0x87b0c223 !addxc	%g3,%g3,%g3
	xor		%l7, %g2, %g2

	.word	0x91b20988 !bshuffle	%f8,%f8,%f8
	.word	0x95b2898a !bshuffle	%f10,%f10,%f10

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= tweak[0]
	.word	0x85b38d82 !fxor	%f14,%f2,%f2
	.word	0x89b20d84 !fxor	%f8,%f4,%f4
	.word	0x8db28d86 !fxor	%f10,%f6,%f6

	brnz,pn		%l2, 2f
	sub		%i2, 2, %i2
		
	std		%f0, [%i1 + 0]
	std		%f2, [%i1 + 8]
	std		%f4, [%i1 + 16]
	std		%f6, [%i1 + 24]
	brnz,pt		%i2, .L128_xts_enloop2x
	add		%i1, 32, %i1

	.word	0x81b00f04 !fsrc2	%f0,%f4,%f0
	.word	0x85b00f06 !fsrc2	%f0,%f6,%f2
	brnz,pn		%i5, .L128_xts_ensteal
	nop

	ret
	restore

.align	16
2:	ldxa		[%i0]0x82, %o0		! avoid read-after-write hazard
						! and ~3x deterioration
						! in inp==out case
	.word	0x91b00900 !faligndata	%f0,%f0,%f8		! handle unaligned output
	.word	0x95b00902 !faligndata	%f0,%f2,%f10
	.word	0x99b08904 !faligndata	%f2,%f4,%f12
	.word	0x9db10906 !faligndata	%f4,%f6,%f14
	.word	0x81b18906 !faligndata	%f6,%f6,%f0

	stda		%f8, [%i1 + %l3]0xc0	! partial store
	std		%f10, [%i1 + 8]
	std		%f12, [%i1 + 16]
	std		%f14, [%i1 + 24]
	add		%i1, 32, %i1
	orn		%g0, %l3, %l3
	stda		%f0, [%i1 + %l3]0xc0	! partial store

	brnz,pt		%i2, .L128_xts_enloop2x+4
	orn		%g0, %l3, %l3

	.word	0x81b00f04 !fsrc2	%f0,%f4,%f0
	.word	0x85b00f06 !fsrc2	%f0,%f6,%f2
	brnz,pn		%i5, .L128_xts_ensteal
	nop

	ret
	restore

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
.align	32
.L128_xts_enblk:
	add	%i1, %i2, %l5
	and	%l5, 63, %l5	! tail
	sub	%i2, %l5, %i2
	add	%l5, 15, %l5	! round up to 16n
	srlx	%i2, 4, %i2
	srl	%l5, 4, %l5
	sub	%i2, 1, %i2
	add	%l5, 1, %l5

.L128_xts_enblk2x:
	ldx		[%i0 + 0], %o0
	ldx		[%i0 + 8], %o1
	ldx		[%i0 + 16], %o2
	brz,pt		%l0, 5f
	ldx		[%i0 + 24], %o3

	ldx		[%i0 + 32], %o4
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	or		%g1, %o0, %o0
	sllx		%o1, %l0, %o1
	srlx		%o2, %l1, %g1
	or		%g1, %o1, %o1
	sllx		%o2, %l0, %o2
	srlx		%o3, %l1, %g1
	or		%g1, %o2, %o2
	sllx		%o3, %l0, %o3
	srlx		%o4, %l1, %o4
	or		%o4, %o3, %o3
5:
	.word	0x99b02302 !movxtod	%g2,%f12
	.word	0x9db02303 !movxtod	%g3,%f14
	.word	0x99b3098c !bshuffle	%f12,%f12,%f12
	.word	0x9db3898e !bshuffle	%f14,%f14,%f14

	srax		%g3, 63, %l7		! next tweak value
	addcc		%g2, %g2, %g2
	and		%l7, 0x87, %l7
	.word	0x87b0c223 !addxc	%g3,%g3,%g3
	xor		%l7, %g2, %g2

	.word	0x91b02302 !movxtod	%g2,%f8
	.word	0x95b02303 !movxtod	%g3,%f10
	.word	0x91b20988 !bshuffle	%f8,%f8,%f8
	.word	0x95b2898a !bshuffle	%f10,%f10,%f10

	xor		%g4, %o0, %o0		! ^= rk[0]
	xor		%g5, %o1, %o1
	xor		%g4, %o2, %o2		! ^= rk[0]
	xor		%g5, %o3, %o3
	.word	0x81b02308 !movxtod	%o0,%f0
	.word	0x85b02309 !movxtod	%o1,%f2
	.word	0x89b0230a !movxtod	%o2,%f4
	.word	0x8db0230b !movxtod	%o3,%f6

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= tweak[0]
	.word	0x85b38d82 !fxor	%f14,%f2,%f2
	.word	0x89b20d84 !fxor	%f8,%f4,%f4		! ^= tweak[0]
	.word	0x8db28d86 !fxor	%f10,%f6,%f6

	prefetch	[%i0 + 32+63], 20
	call		_aes128_encrypt_2x
	add		%i0, 32, %i0

	.word	0x91b02302 !movxtod	%g2,%f8
	.word	0x95b02303 !movxtod	%g3,%f10

	srax		%g3, 63, %l7		! next tweak value
	addcc		%g2, %g2, %g2
	and		%l7, 0x87, %l7
	.word	0x87b0c223 !addxc	%g3,%g3,%g3
	xor		%l7, %g2, %g2

	.word	0x91b20988 !bshuffle	%f8,%f8,%f8
	.word	0x95b2898a !bshuffle	%f10,%f10,%f10

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= tweak[0]
	.word	0x85b38d82 !fxor	%f14,%f2,%f2
	.word	0x89b20d84 !fxor	%f8,%f4,%f4
	.word	0x8db28d86 !fxor	%f10,%f6,%f6

	subcc		%i2, 2, %i2
	stda		%f0, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	add		%i1, 8, %i1
	stda		%f2, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	add		%i1, 8, %i1
	stda		%f4, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	add		%i1, 8, %i1
	stda		%f6, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	bgu,pt		%xcc, .L128_xts_enblk2x
	add		%i1, 8, %i1

	add		%l5, %i2, %i2
	andcc		%i2, 1, %g0		! is number of blocks even?
	membar		#StoreLoad|#StoreStore
	bnz,pt		%icc, .L128_xts_enloop
	srl		%i2, 0, %i2
	brnz,pn		%i2, .L128_xts_enloop2x
	nop

	.word	0x81b00f04 !fsrc2	%f0,%f4,%f0
	.word	0x85b00f06 !fsrc2	%f0,%f6,%f2
	brnz,pn		%i5, .L128_xts_ensteal
	nop

	ret
	restore
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
.align	32
.L128_xts_ensteal:
	std		%f0, [%fp + 2047-16]	! copy of output
	std		%f2, [%fp + 2047-8]

	srl		%l0, 3, %l0
	add		%fp, 2047-16, %l7
	add		%i0, %l0, %i0	! original %i0+%i2&-15
	add		%i1, %l2, %i1	! original %i1+%i2&-15
	mov		0, %l0
	nop					! align

.L128_xts_enstealing:
	ldub		[%i0 + %l0], %o0
	ldub		[%l7  + %l0], %o1
	dec		%i5
	stb		%o0, [%l7  + %l0]
	stb		%o1, [%i1 + %l0]
	brnz		%i5, .L128_xts_enstealing
	inc		%l0

	mov		%l7, %i0
	sub		%i1, 16, %i1
	mov		0, %l0
	sub		%i1, %l2, %i1
	ba		.L128_xts_enloop	! one more time
	mov		1, %i2				! %i5 is 0
	ret
	restore
.type	aes128_t4_xts_encrypt,#function
.size	aes128_t4_xts_encrypt,.-aes128_t4_xts_encrypt
.globl	aes128_t4_xts_decrypt
.align	32
aes128_t4_xts_decrypt:
	save		%sp, -192-16, %sp
	srln		%i2, 0, %i2		! needed on v8+, "nop" on v9

	mov		%i5, %o0
	add		%fp, 2047-16, %o1
	call		aes_t4_encrypt
	mov		%i4, %o2

	add		%fp, 2047-16, %l7
	ldxa		[%l7]0x88, %g2
	add		%fp, 2047-8, %l7
	ldxa		[%l7]0x88, %g3		! %g3:%g2 is tweak

	sethi		%hi(0x76543210), %l7
	or		%l7, %lo(0x76543210), %l7
	.word	0x81b5c320 !bmask	%l7,%g0,%g0		! byte swap mask

	prefetch	[%i0], 20
	prefetch	[%i0 + 63], 20
	call		_aes128_load_deckey
	and		%i2, 15,  %i5
	and		%i2, -16, %i2
	mov		0, %l7
	movrnz		%i5, 16,  %l7
	sub		%i2, %l7, %i2

	sub		%i0, %i1, %l5	! %i0!=%i1
	and		%i0, 7, %l0
	andn		%i0, 7, %i0
	sll		%l0, 3, %l0
	mov		64, %l1
	mov		0xff, %l3
	sub		%l1, %l0, %l1
	and		%i1, 7, %l2
	cmp		%i2, 255
	movrnz		%l2, 0, %l5		! if (	%i1&7 ||
	movleu		%xcc, 0, %l5	!	%i2<256 ||
	brnz,pn		%l5, .L128_xts_deblk !	%i0==%i1)
	srl		%l3, %l2, %l3

	andcc		%i2, 16, %g0		! is number of blocks even?
	brz,pn		%i2, .L128_xts_desteal
	.word	0xb3b64340 !alignaddrl	%i1,%g0,%i1
	bz		%icc, .L128_xts_deloop2x
	srlx		%i2, 4, %i2
.L128_xts_deloop:
	ldx		[%i0 + 0], %o0
	brz,pt		%l0, 4f
	ldx		[%i0 + 8], %o1

	ldx		[%i0 + 16], %o2
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	sllx		%o1, %l0, %o1
	or		%g1, %o0, %o0
	srlx		%o2, %l1, %o2
	or		%o2, %o1, %o1
4:
	.word	0x99b02302 !movxtod	%g2,%f12
	.word	0x9db02303 !movxtod	%g3,%f14
	.word	0x99b3098c !bshuffle	%f12,%f12,%f12
	.word	0x9db3898e !bshuffle	%f14,%f14,%f14

	xor		%g4, %o0, %o0		! ^= rk[0]
	xor		%g5, %o1, %o1
	.word	0x81b02308 !movxtod	%o0,%f0
	.word	0x85b02309 !movxtod	%o1,%f2

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= tweak[0]
	.word	0x85b38d82 !fxor	%f14,%f2,%f2

	prefetch	[%i1 + 63], 22
	prefetch	[%i0 + 16+63], 20
	call		_aes128_decrypt_1x
	add		%i0, 16, %i0

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= tweak[0]
	.word	0x85b38d82 !fxor	%f14,%f2,%f2

	srax		%g3, 63, %l7		! next tweak value
	addcc		%g2, %g2, %g2
	and		%l7, 0x87, %l7
	.word	0x87b0c223 !addxc	%g3,%g3,%g3
	xor		%l7, %g2, %g2

	brnz,pn		%l2, 2f
	sub		%i2, 1, %i2
		
	std		%f0, [%i1 + 0]
	std		%f2, [%i1 + 8]
	brnz,pt		%i2, .L128_xts_deloop2x
	add		%i1, 16, %i1

	brnz,pn		%i5, .L128_xts_desteal
	nop

	ret
	restore

.align	16
2:	ldxa		[%i0]0x82, %o0		! avoid read-after-write hazard
						! and ~3x deterioration
						! in inp==out case
	.word	0x89b00900 !faligndata	%f0,%f0,%f4		! handle unaligned output
	.word	0x8db00902 !faligndata	%f0,%f2,%f6
	.word	0x91b08902 !faligndata	%f2,%f2,%f8
	stda		%f4, [%i1 + %l3]0xc0	! partial store
	std		%f6, [%i1 + 8]
	add		%i1, 16, %i1
	orn		%g0, %l3, %l3
	stda		%f8, [%i1 + %l3]0xc0	! partial store

	brnz,pt		%i2, .L128_xts_deloop2x+4
	orn		%g0, %l3, %l3

	brnz,pn		%i5, .L128_xts_desteal
	nop

	ret
	restore

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
.align	32
.L128_xts_deloop2x:
	ldx		[%i0 + 0], %o0
	ldx		[%i0 + 8], %o1
	ldx		[%i0 + 16], %o2
	brz,pt		%l0, 4f
	ldx		[%i0 + 24], %o3

	ldx		[%i0 + 32], %o4
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	or		%g1, %o0, %o0
	sllx		%o1, %l0, %o1
	srlx		%o2, %l1, %g1
	or		%g1, %o1, %o1
	sllx		%o2, %l0, %o2
	srlx		%o3, %l1, %g1
	or		%g1, %o2, %o2
	sllx		%o3, %l0, %o3
	srlx		%o4, %l1, %o4
	or		%o4, %o3, %o3
4:
	.word	0x99b02302 !movxtod	%g2,%f12
	.word	0x9db02303 !movxtod	%g3,%f14
	.word	0x99b3098c !bshuffle	%f12,%f12,%f12
	.word	0x9db3898e !bshuffle	%f14,%f14,%f14

	srax		%g3, 63, %l7		! next tweak value
	addcc		%g2, %g2, %g2
	and		%l7, 0x87, %l7
	.word	0x87b0c223 !addxc	%g3,%g3,%g3
	xor		%l7, %g2, %g2

	.word	0x91b02302 !movxtod	%g2,%f8
	.word	0x95b02303 !movxtod	%g3,%f10
	.word	0x91b20988 !bshuffle	%f8,%f8,%f8
	.word	0x95b2898a !bshuffle	%f10,%f10,%f10

	xor		%g4, %o0, %o0		! ^= rk[0]
	xor		%g5, %o1, %o1
	xor		%g4, %o2, %o2		! ^= rk[0]
	xor		%g5, %o3, %o3
	.word	0x81b02308 !movxtod	%o0,%f0
	.word	0x85b02309 !movxtod	%o1,%f2
	.word	0x89b0230a !movxtod	%o2,%f4
	.word	0x8db0230b !movxtod	%o3,%f6

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= tweak[0]
	.word	0x85b38d82 !fxor	%f14,%f2,%f2
	.word	0x89b20d84 !fxor	%f8,%f4,%f4		! ^= tweak[0]
	.word	0x8db28d86 !fxor	%f10,%f6,%f6

	prefetch	[%i1 + 63], 22
	prefetch	[%i0 + 32+63], 20
	call		_aes128_decrypt_2x
	add		%i0, 32, %i0

	.word	0x91b02302 !movxtod	%g2,%f8
	.word	0x95b02303 !movxtod	%g3,%f10

	srax		%g3, 63, %l7		! next tweak value
	addcc		%g2, %g2, %g2
	and		%l7, 0x87, %l7
	.word	0x87b0c223 !addxc	%g3,%g3,%g3
	xor		%l7, %g2, %g2

	.word	0x91b20988 !bshuffle	%f8,%f8,%f8
	.word	0x95b2898a !bshuffle	%f10,%f10,%f10

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= tweak[0]
	.word	0x85b38d82 !fxor	%f14,%f2,%f2
	.word	0x89b20d84 !fxor	%f8,%f4,%f4
	.word	0x8db28d86 !fxor	%f10,%f6,%f6

	brnz,pn		%l2, 2f
	sub		%i2, 2, %i2
		
	std		%f0, [%i1 + 0]
	std		%f2, [%i1 + 8]
	std		%f4, [%i1 + 16]
	std		%f6, [%i1 + 24]
	brnz,pt		%i2, .L128_xts_deloop2x
	add		%i1, 32, %i1

	.word	0x81b00f04 !fsrc2	%f0,%f4,%f0
	.word	0x85b00f06 !fsrc2	%f0,%f6,%f2
	brnz,pn		%i5, .L128_xts_desteal
	nop

	ret
	restore

.align	16
2:	ldxa		[%i0]0x82, %o0		! avoid read-after-write hazard
						! and ~3x deterioration
						! in inp==out case
	.word	0x91b00900 !faligndata	%f0,%f0,%f8		! handle unaligned output
	.word	0x95b00902 !faligndata	%f0,%f2,%f10
	.word	0x99b08904 !faligndata	%f2,%f4,%f12
	.word	0x9db10906 !faligndata	%f4,%f6,%f14
	.word	0x81b18906 !faligndata	%f6,%f6,%f0

	stda		%f8, [%i1 + %l3]0xc0	! partial store
	std		%f10, [%i1 + 8]
	std		%f12, [%i1 + 16]
	std		%f14, [%i1 + 24]
	add		%i1, 32, %i1
	orn		%g0, %l3, %l3
	stda		%f0, [%i1 + %l3]0xc0	! partial store

	brnz,pt		%i2, .L128_xts_deloop2x+4
	orn		%g0, %l3, %l3

	.word	0x81b00f04 !fsrc2	%f0,%f4,%f0
	.word	0x85b00f06 !fsrc2	%f0,%f6,%f2
	brnz,pn		%i5, .L128_xts_desteal
	nop

	ret
	restore

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
.align	32
.L128_xts_deblk:
	add	%i1, %i2, %l5
	and	%l5, 63, %l5	! tail
	sub	%i2, %l5, %i2
	add	%l5, 15, %l5	! round up to 16n
	srlx	%i2, 4, %i2
	srl	%l5, 4, %l5
	sub	%i2, 1, %i2
	add	%l5, 1, %l5

.L128_xts_deblk2x:
	ldx		[%i0 + 0], %o0
	ldx		[%i0 + 8], %o1
	ldx		[%i0 + 16], %o2
	brz,pt		%l0, 5f
	ldx		[%i0 + 24], %o3

	ldx		[%i0 + 32], %o4
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	or		%g1, %o0, %o0
	sllx		%o1, %l0, %o1
	srlx		%o2, %l1, %g1
	or		%g1, %o1, %o1
	sllx		%o2, %l0, %o2
	srlx		%o3, %l1, %g1
	or		%g1, %o2, %o2
	sllx		%o3, %l0, %o3
	srlx		%o4, %l1, %o4
	or		%o4, %o3, %o3
5:
	.word	0x99b02302 !movxtod	%g2,%f12
	.word	0x9db02303 !movxtod	%g3,%f14
	.word	0x99b3098c !bshuffle	%f12,%f12,%f12
	.word	0x9db3898e !bshuffle	%f14,%f14,%f14

	srax		%g3, 63, %l7		! next tweak value
	addcc		%g2, %g2, %g2
	and		%l7, 0x87, %l7
	.word	0x87b0c223 !addxc	%g3,%g3,%g3
	xor		%l7, %g2, %g2

	.word	0x91b02302 !movxtod	%g2,%f8
	.word	0x95b02303 !movxtod	%g3,%f10
	.word	0x91b20988 !bshuffle	%f8,%f8,%f8
	.word	0x95b2898a !bshuffle	%f10,%f10,%f10

	xor		%g4, %o0, %o0		! ^= rk[0]
	xor		%g5, %o1, %o1
	xor		%g4, %o2, %o2		! ^= rk[0]
	xor		%g5, %o3, %o3
	.word	0x81b02308 !movxtod	%o0,%f0
	.word	0x85b02309 !movxtod	%o1,%f2
	.word	0x89b0230a !movxtod	%o2,%f4
	.word	0x8db0230b !movxtod	%o3,%f6

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= tweak[0]
	.word	0x85b38d82 !fxor	%f14,%f2,%f2
	.word	0x89b20d84 !fxor	%f8,%f4,%f4		! ^= tweak[0]
	.word	0x8db28d86 !fxor	%f10,%f6,%f6

	prefetch	[%i0 + 32+63], 20
	call		_aes128_decrypt_2x
	add		%i0, 32, %i0

	.word	0x91b02302 !movxtod	%g2,%f8
	.word	0x95b02303 !movxtod	%g3,%f10

	srax		%g3, 63, %l7		! next tweak value
	addcc		%g2, %g2, %g2
	and		%l7, 0x87, %l7
	.word	0x87b0c223 !addxc	%g3,%g3,%g3
	xor		%l7, %g2, %g2

	.word	0x91b20988 !bshuffle	%f8,%f8,%f8
	.word	0x95b2898a !bshuffle	%f10,%f10,%f10

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= tweak[0]
	.word	0x85b38d82 !fxor	%f14,%f2,%f2
	.word	0x89b20d84 !fxor	%f8,%f4,%f4
	.word	0x8db28d86 !fxor	%f10,%f6,%f6

	subcc		%i2, 2, %i2
	stda		%f0, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	add		%i1, 8, %i1
	stda		%f2, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	add		%i1, 8, %i1
	stda		%f4, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	add		%i1, 8, %i1
	stda		%f6, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	bgu,pt		%xcc, .L128_xts_deblk2x
	add		%i1, 8, %i1

	add		%l5, %i2, %i2
	andcc		%i2, 1, %g0		! is number of blocks even?
	membar		#StoreLoad|#StoreStore
	bnz,pt		%icc, .L128_xts_deloop
	srl		%i2, 0, %i2
	brnz,pn		%i2, .L128_xts_deloop2x
	nop

	.word	0x81b00f04 !fsrc2	%f0,%f4,%f0
	.word	0x85b00f06 !fsrc2	%f0,%f6,%f2
	brnz,pn		%i5, .L128_xts_desteal
	nop

	ret
	restore
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
.align	32
.L128_xts_desteal:
	ldx		[%i0 + 0], %o0
	brz,pt		%l0, 8f
	ldx		[%i0 + 8], %o1

	ldx		[%i0 + 16], %o2
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	sllx		%o1, %l0, %o1
	or		%g1, %o0, %o0
	srlx		%o2, %l1, %o2
	or		%o2, %o1, %o1
8:
	srax		%g3, 63, %l7		! next tweak value
	addcc		%g2, %g2, %o2
	and		%l7, 0x87, %l7
	.word	0x97b0c223 !addxc	%g3,%g3,%o3
	xor		%l7, %o2, %o2

	.word	0x99b0230a !movxtod	%o2,%f12
	.word	0x9db0230b !movxtod	%o3,%f14
	.word	0x99b3098c !bshuffle	%f12,%f12,%f12
	.word	0x9db3898e !bshuffle	%f14,%f14,%f14

	xor		%g4, %o0, %o0		! ^= rk[0]
	xor		%g5, %o1, %o1
	.word	0x81b02308 !movxtod	%o0,%f0
	.word	0x85b02309 !movxtod	%o1,%f2

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= tweak[0]
	.word	0x85b38d82 !fxor	%f14,%f2,%f2

	call		_aes128_decrypt_1x
	add		%i0, 16, %i0

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= tweak[0]
	.word	0x85b38d82 !fxor	%f14,%f2,%f2

	std		%f0, [%fp + 2047-16]
	std		%f2, [%fp + 2047-8]

	srl		%l0, 3, %l0
	add		%fp, 2047-16, %l7
	add		%i0, %l0, %i0	! original %i0+%i2&-15
	add		%i1, %l2, %i1	! original %i1+%i2&-15
	mov		0, %l0
	add		%i1, 16, %i1
	nop					! align

.L128_xts_destealing:
	ldub		[%i0 + %l0], %o0
	ldub		[%l7  + %l0], %o1
	dec		%i5
	stb		%o0, [%l7  + %l0]
	stb		%o1, [%i1 + %l0]
	brnz		%i5, .L128_xts_destealing
	inc		%l0

	mov		%l7, %i0
	sub		%i1, 16, %i1
	mov		0, %l0
	sub		%i1, %l2, %i1
	ba		.L128_xts_deloop	! one more time
	mov		1, %i2				! %i5 is 0
	ret
	restore
.type	aes128_t4_xts_decrypt,#function
.size	aes128_t4_xts_decrypt,.-aes128_t4_xts_decrypt
.globl	aes128_t4_cbc_decrypt
.align	32
aes128_t4_cbc_decrypt:
	save		%sp, -192, %sp
	cmp		%i2, 0
	be,pn		%xcc, .L128_cbc_dec_abort
	srln		%i2, 0, %i2		! needed on v8+, "nop" on v9
	sub		%i0, %i1, %l5	! %i0!=%i1
	ld		[%i4 + 0], %f12	! load ivec
	ld		[%i4 + 4], %f13
	ld		[%i4 + 8], %f14
	ld		[%i4 + 12], %f15
	prefetch	[%i0], 20
	prefetch	[%i0 + 63], 20
	call		_aes128_load_deckey
	and		%i0, 7, %l0
	andn		%i0, 7, %i0
	sll		%l0, 3, %l0
	mov		64, %l1
	mov		0xff, %l3
	sub		%l1, %l0, %l1
	and		%i1, 7, %l2
	cmp		%i2, 255
	movrnz		%l2, 0, %l5		! if (	%i1&7 ||
	movleu		%xcc, 0, %l5	!	%i2<256 ||
	brnz,pn		%l5, .L128cbc_dec_blk	!	%i0==%i1)
	srl		%l3, %l2, %l3

	andcc		%i2, 16, %g0		! is number of blocks even?
	srlx		%i2, 4, %i2
	.word	0xb3b64340 !alignaddrl	%i1,%g0,%i1
	bz		%icc, .L128_cbc_dec_loop2x
	prefetch	[%i1], 22
.L128_cbc_dec_loop:
	ldx		[%i0 + 0], %o0
	brz,pt		%l0, 4f
	ldx		[%i0 + 8], %o1

	ldx		[%i0 + 16], %o2
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	sllx		%o1, %l0, %o1
	or		%g1, %o0, %o0
	srlx		%o2, %l1, %o2
	or		%o2, %o1, %o1
4:
	xor		%g4, %o0, %o2		! ^= rk[0]
	xor		%g5, %o1, %o3
	.word	0x81b0230a !movxtod	%o2,%f0
	.word	0x85b0230b !movxtod	%o3,%f2

	prefetch	[%i1 + 63], 22
	prefetch	[%i0 + 16+63], 20
	call		_aes128_decrypt_1x
	add		%i0, 16, %i0

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= ivec
	.word	0x85b38d82 !fxor	%f14,%f2,%f2
	.word	0x99b02308 !movxtod	%o0,%f12
	.word	0x9db02309 !movxtod	%o1,%f14

	brnz,pn		%l2, 2f
	sub		%i2, 1, %i2
		
	std		%f0, [%i1 + 0]
	std		%f2, [%i1 + 8]
	brnz,pt		%i2, .L128_cbc_dec_loop2x
	add		%i1, 16, %i1
	st		%f12, [%i4 + 0]
	st		%f13, [%i4 + 4]
	st		%f14, [%i4 + 8]
	st		%f15, [%i4 + 12]
.L128_cbc_dec_abort:
	ret
	restore

.align	16
2:	ldxa		[%i0]0x82, %o0		! avoid read-after-write hazard
						! and ~3x deterioration
						! in inp==out case
	.word	0x89b00900 !faligndata	%f0,%f0,%f4		! handle unaligned output
	.word	0x8db00902 !faligndata	%f0,%f2,%f6
	.word	0x91b08902 !faligndata	%f2,%f2,%f8

	stda		%f4, [%i1 + %l3]0xc0	! partial store
	std		%f6, [%i1 + 8]
	add		%i1, 16, %i1
	orn		%g0, %l3, %l3
	stda		%f8, [%i1 + %l3]0xc0	! partial store

	brnz,pt		%i2, .L128_cbc_dec_loop2x+4
	orn		%g0, %l3, %l3
	st		%f12, [%i4 + 0]
	st		%f13, [%i4 + 4]
	st		%f14, [%i4 + 8]
	st		%f15, [%i4 + 12]
	ret
	restore

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
.align	32
.L128_cbc_dec_loop2x:
	ldx		[%i0 + 0], %o0
	ldx		[%i0 + 8], %o1
	ldx		[%i0 + 16], %o2
	brz,pt		%l0, 4f
	ldx		[%i0 + 24], %o3

	ldx		[%i0 + 32], %o4
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	or		%g1, %o0, %o0
	sllx		%o1, %l0, %o1
	srlx		%o2, %l1, %g1
	or		%g1, %o1, %o1
	sllx		%o2, %l0, %o2
	srlx		%o3, %l1, %g1
	or		%g1, %o2, %o2
	sllx		%o3, %l0, %o3
	srlx		%o4, %l1, %o4
	or		%o4, %o3, %o3
4:
	xor		%g4, %o0, %o4		! ^= rk[0]
	xor		%g5, %o1, %o5
	.word	0x81b0230c !movxtod	%o4,%f0
	.word	0x85b0230d !movxtod	%o5,%f2
	xor		%g4, %o2, %o4
	xor		%g5, %o3, %o5
	.word	0x89b0230c !movxtod	%o4,%f4
	.word	0x8db0230d !movxtod	%o5,%f6

	prefetch	[%i1 + 63], 22
	prefetch	[%i0 + 32+63], 20
	call		_aes128_decrypt_2x
	add		%i0, 32, %i0

	.word	0x91b02308 !movxtod	%o0,%f8
	.word	0x95b02309 !movxtod	%o1,%f10
	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= ivec
	.word	0x85b38d82 !fxor	%f14,%f2,%f2
	.word	0x99b0230a !movxtod	%o2,%f12
	.word	0x9db0230b !movxtod	%o3,%f14
	.word	0x89b20d84 !fxor	%f8,%f4,%f4
	.word	0x8db28d86 !fxor	%f10,%f6,%f6

	brnz,pn		%l2, 2f
	sub		%i2, 2, %i2
		
	std		%f0, [%i1 + 0]
	std		%f2, [%i1 + 8]
	std		%f4, [%i1 + 16]
	std		%f6, [%i1 + 24]
	brnz,pt		%i2, .L128_cbc_dec_loop2x
	add		%i1, 32, %i1
	st		%f12, [%i4 + 0]
	st		%f13, [%i4 + 4]
	st		%f14, [%i4 + 8]
	st		%f15, [%i4 + 12]
	ret
	restore

.align	16
2:	ldxa		[%i0]0x82, %o0		! avoid read-after-write hazard
						! and ~3x deterioration
						! in inp==out case
	.word	0x91b00900 !faligndata	%f0,%f0,%f8		! handle unaligned output
	.word	0x81b00902 !faligndata	%f0,%f2,%f0
	.word	0x85b08904 !faligndata	%f2,%f4,%f2
	.word	0x89b10906 !faligndata	%f4,%f6,%f4
	.word	0x8db18906 !faligndata	%f6,%f6,%f6
	stda		%f8, [%i1 + %l3]0xc0	! partial store
	std		%f0, [%i1 + 8]
	std		%f2, [%i1 + 16]
	std		%f4, [%i1 + 24]
	add		%i1, 32, %i1
	orn		%g0, %l3, %l3
	stda		%f6, [%i1 + %l3]0xc0	! partial store

	brnz,pt		%i2, .L128_cbc_dec_loop2x+4
	orn		%g0, %l3, %l3
	st		%f12, [%i4 + 0]
	st		%f13, [%i4 + 4]
	st		%f14, [%i4 + 8]
	st		%f15, [%i4 + 12]
	ret
	restore

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
.align	32
.L128cbc_dec_blk:
	add	%i1, %i2, %l5
	and	%l5, 63, %l5	! tail
	sub	%i2, %l5, %i2
	add	%l5, 15, %l5	! round up to 16n
	srlx	%i2, 4, %i2
	srl	%l5, 4, %l5
	sub	%i2, 1, %i2
	add	%l5, 1, %l5

.L128_cbc_dec_blk_loop2x:
	ldx		[%i0 + 0], %o0
	ldx		[%i0 + 8], %o1
	ldx		[%i0 + 16], %o2
	brz,pt		%l0, 5f
	ldx		[%i0 + 24], %o3

	ldx		[%i0 + 32], %o4
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	or		%g1, %o0, %o0
	sllx		%o1, %l0, %o1
	srlx		%o2, %l1, %g1
	or		%g1, %o1, %o1
	sllx		%o2, %l0, %o2
	srlx		%o3, %l1, %g1
	or		%g1, %o2, %o2
	sllx		%o3, %l0, %o3
	srlx		%o4, %l1, %o4
	or		%o4, %o3, %o3
5:
	xor		%g4, %o0, %o4		! ^= rk[0]
	xor		%g5, %o1, %o5
	.word	0x81b0230c !movxtod	%o4,%f0
	.word	0x85b0230d !movxtod	%o5,%f2
	xor		%g4, %o2, %o4
	xor		%g5, %o3, %o5
	.word	0x89b0230c !movxtod	%o4,%f4
	.word	0x8db0230d !movxtod	%o5,%f6

	prefetch	[%i0 + 32+63], 20
	call		_aes128_decrypt_2x
	add		%i0, 32, %i0
	subcc		%i2, 2, %i2

	.word	0x91b02308 !movxtod	%o0,%f8
	.word	0x95b02309 !movxtod	%o1,%f10
	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= ivec
	.word	0x85b38d82 !fxor	%f14,%f2,%f2
	.word	0x99b0230a !movxtod	%o2,%f12
	.word	0x9db0230b !movxtod	%o3,%f14
	.word	0x89b20d84 !fxor	%f8,%f4,%f4
	.word	0x8db28d86 !fxor	%f10,%f6,%f6

	stda		%f0, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	add		%i1, 8, %i1
	stda		%f2, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	add		%i1, 8, %i1
	stda		%f4, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	add		%i1, 8, %i1
	stda		%f6, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	bgu,pt		%xcc, .L128_cbc_dec_blk_loop2x
	add		%i1, 8, %i1

	add		%l5, %i2, %i2
	andcc		%i2, 1, %g0		! is number of blocks even?
	membar		#StoreLoad|#StoreStore
	bnz,pt		%icc, .L128_cbc_dec_loop
	srl		%i2, 0, %i2
	brnz,pn		%i2, .L128_cbc_dec_loop2x
	nop
	st		%f12, [%i4 + 0]	! write out ivec
	st		%f13, [%i4 + 4]
	st		%f14, [%i4 + 8]
	st		%f15, [%i4 + 12]
	ret
	restore
.type	aes128_t4_cbc_decrypt,#function
.size	aes128_t4_cbc_decrypt,.-aes128_t4_cbc_decrypt
.align	32
_aes128_decrypt_1x:
	.word	0x88cc0440 !aes_dround01	%f16,%f0,%f2,%f4
	.word	0x84cc8460 !aes_dround23	%f18,%f0,%f2,%f2
	.word	0x80cd0444 !aes_dround01	%f20,%f4,%f2,%f0
	.word	0x84cd8464 !aes_dround23	%f22,%f4,%f2,%f2
	.word	0x88ce0440 !aes_dround01	%f24,%f0,%f2,%f4
	.word	0x84ce8460 !aes_dround23	%f26,%f0,%f2,%f2
	.word	0x80cf0444 !aes_dround01	%f28,%f4,%f2,%f0
	.word	0x84cf8464 !aes_dround23	%f30,%f4,%f2,%f2
	.word	0x88c84440 !aes_dround01	%f32,%f0,%f2,%f4
	.word	0x84c8c460 !aes_dround23	%f34,%f0,%f2,%f2
	.word	0x80c94444 !aes_dround01	%f36,%f4,%f2,%f0
	.word	0x84c9c464 !aes_dround23	%f38,%f4,%f2,%f2
	.word	0x88ca4440 !aes_dround01	%f40,%f0,%f2,%f4
	.word	0x84cac460 !aes_dround23	%f42,%f0,%f2,%f2
	.word	0x80cb4444 !aes_dround01	%f44,%f4,%f2,%f0
	.word	0x84cbc464 !aes_dround23	%f46,%f4,%f2,%f2
	.word	0x88cc4440 !aes_dround01	%f48,%f0,%f2,%f4
	.word	0x84ccc460 !aes_dround23	%f50,%f0,%f2,%f2
	.word	0x80cd44c4 !aes_dround01_l	%f52,%f4,%f2,%f0
	retl
	.word	0x84cdc4e4 !aes_dround23_l	%f54,%f4,%f2,%f2
.type	_aes128_decrypt_1x,#function
.size	_aes128_decrypt_1x,.-_aes128_decrypt_1x

.align	32
_aes128_decrypt_2x:
	.word	0x90cc0440 !aes_dround01	%f16,%f0,%f2,%f8
	.word	0x84cc8460 !aes_dround23	%f18,%f0,%f2,%f2
	.word	0x94cc0c44 !aes_dround01	%f16,%f4,%f6,%f10
	.word	0x8ccc8c64 !aes_dround23	%f18,%f4,%f6,%f6
	.word	0x80cd0448 !aes_dround01	%f20,%f8,%f2,%f0
	.word	0x84cd8468 !aes_dround23	%f22,%f8,%f2,%f2
	.word	0x88cd0c4a !aes_dround01	%f20,%f10,%f6,%f4
	.word	0x8ccd8c6a !aes_dround23	%f22,%f10,%f6,%f6
	.word	0x90ce0440 !aes_dround01	%f24,%f0,%f2,%f8
	.word	0x84ce8460 !aes_dround23	%f26,%f0,%f2,%f2
	.word	0x94ce0c44 !aes_dround01	%f24,%f4,%f6,%f10
	.word	0x8cce8c64 !aes_dround23	%f26,%f4,%f6,%f6
	.word	0x80cf0448 !aes_dround01	%f28,%f8,%f2,%f0
	.word	0x84cf8468 !aes_dround23	%f30,%f8,%f2,%f2
	.word	0x88cf0c4a !aes_dround01	%f28,%f10,%f6,%f4
	.word	0x8ccf8c6a !aes_dround23	%f30,%f10,%f6,%f6
	.word	0x90c84440 !aes_dround01	%f32,%f0,%f2,%f8
	.word	0x84c8c460 !aes_dround23	%f34,%f0,%f2,%f2
	.word	0x94c84c44 !aes_dround01	%f32,%f4,%f6,%f10
	.word	0x8cc8cc64 !aes_dround23	%f34,%f4,%f6,%f6
	.word	0x80c94448 !aes_dround01	%f36,%f8,%f2,%f0
	.word	0x84c9c468 !aes_dround23	%f38,%f8,%f2,%f2
	.word	0x88c94c4a !aes_dround01	%f36,%f10,%f6,%f4
	.word	0x8cc9cc6a !aes_dround23	%f38,%f10,%f6,%f6
	.word	0x90ca4440 !aes_dround01	%f40,%f0,%f2,%f8
	.word	0x84cac460 !aes_dround23	%f42,%f0,%f2,%f2
	.word	0x94ca4c44 !aes_dround01	%f40,%f4,%f6,%f10
	.word	0x8ccacc64 !aes_dround23	%f42,%f4,%f6,%f6
	.word	0x80cb4448 !aes_dround01	%f44,%f8,%f2,%f0
	.word	0x84cbc468 !aes_dround23	%f46,%f8,%f2,%f2
	.word	0x88cb4c4a !aes_dround01	%f44,%f10,%f6,%f4
	.word	0x8ccbcc6a !aes_dround23	%f46,%f10,%f6,%f6
	.word	0x90cc4440 !aes_dround01	%f48,%f0,%f2,%f8
	.word	0x84ccc460 !aes_dround23	%f50,%f0,%f2,%f2
	.word	0x94cc4c44 !aes_dround01	%f48,%f4,%f6,%f10
	.word	0x8ccccc64 !aes_dround23	%f50,%f4,%f6,%f6
	.word	0x80cd44c8 !aes_dround01_l	%f52,%f8,%f2,%f0
	.word	0x84cdc4e8 !aes_dround23_l	%f54,%f8,%f2,%f2
	.word	0x88cd4cca !aes_dround01_l	%f52,%f10,%f6,%f4
	retl
	.word	0x8ccdccea !aes_dround23_l	%f54,%f10,%f6,%f6
.type	_aes128_decrypt_2x,#function
.size	_aes128_decrypt_2x,.-_aes128_decrypt_2x
.align	32
_aes192_encrypt_1x:
	.word	0x88cc0400 !aes_eround01	%f16,%f0,%f2,%f4
	.word	0x84cc8420 !aes_eround23	%f18,%f0,%f2,%f2
	.word	0x80cd0404 !aes_eround01	%f20,%f4,%f2,%f0
	.word	0x84cd8424 !aes_eround23	%f22,%f4,%f2,%f2
	.word	0x88ce0400 !aes_eround01	%f24,%f0,%f2,%f4
	.word	0x84ce8420 !aes_eround23	%f26,%f0,%f2,%f2
	.word	0x80cf0404 !aes_eround01	%f28,%f4,%f2,%f0
	.word	0x84cf8424 !aes_eround23	%f30,%f4,%f2,%f2
	.word	0x88c84400 !aes_eround01	%f32,%f0,%f2,%f4
	.word	0x84c8c420 !aes_eround23	%f34,%f0,%f2,%f2
	.word	0x80c94404 !aes_eround01	%f36,%f4,%f2,%f0
	.word	0x84c9c424 !aes_eround23	%f38,%f4,%f2,%f2
	.word	0x88ca4400 !aes_eround01	%f40,%f0,%f2,%f4
	.word	0x84cac420 !aes_eround23	%f42,%f0,%f2,%f2
	.word	0x80cb4404 !aes_eround01	%f44,%f4,%f2,%f0
	.word	0x84cbc424 !aes_eround23	%f46,%f4,%f2,%f2
	.word	0x88cc4400 !aes_eround01	%f48,%f0,%f2,%f4
	.word	0x84ccc420 !aes_eround23	%f50,%f0,%f2,%f2
	.word	0x80cd4404 !aes_eround01	%f52,%f4,%f2,%f0
	.word	0x84cdc424 !aes_eround23	%f54,%f4,%f2,%f2
	.word	0x88ce4400 !aes_eround01	%f56,%f0,%f2,%f4
	.word	0x84cec420 !aes_eround23	%f58,%f0,%f2,%f2
	.word	0x80cf4484 !aes_eround01_l	%f60,%f4,%f2,%f0
	retl
	.word	0x84cfc4a4 !aes_eround23_l	%f62,%f4,%f2,%f2
.type	_aes192_encrypt_1x,#function
.size	_aes192_encrypt_1x,.-_aes192_encrypt_1x

.align	32
_aes192_encrypt_2x:
	.word	0x90cc0400 !aes_eround01	%f16,%f0,%f2,%f8
	.word	0x84cc8420 !aes_eround23	%f18,%f0,%f2,%f2
	.word	0x94cc0c04 !aes_eround01	%f16,%f4,%f6,%f10
	.word	0x8ccc8c24 !aes_eround23	%f18,%f4,%f6,%f6
	.word	0x80cd0408 !aes_eround01	%f20,%f8,%f2,%f0
	.word	0x84cd8428 !aes_eround23	%f22,%f8,%f2,%f2
	.word	0x88cd0c0a !aes_eround01	%f20,%f10,%f6,%f4
	.word	0x8ccd8c2a !aes_eround23	%f22,%f10,%f6,%f6
	.word	0x90ce0400 !aes_eround01	%f24,%f0,%f2,%f8
	.word	0x84ce8420 !aes_eround23	%f26,%f0,%f2,%f2
	.word	0x94ce0c04 !aes_eround01	%f24,%f4,%f6,%f10
	.word	0x8cce8c24 !aes_eround23	%f26,%f4,%f6,%f6
	.word	0x80cf0408 !aes_eround01	%f28,%f8,%f2,%f0
	.word	0x84cf8428 !aes_eround23	%f30,%f8,%f2,%f2
	.word	0x88cf0c0a !aes_eround01	%f28,%f10,%f6,%f4
	.word	0x8ccf8c2a !aes_eround23	%f30,%f10,%f6,%f6
	.word	0x90c84400 !aes_eround01	%f32,%f0,%f2,%f8
	.word	0x84c8c420 !aes_eround23	%f34,%f0,%f2,%f2
	.word	0x94c84c04 !aes_eround01	%f32,%f4,%f6,%f10
	.word	0x8cc8cc24 !aes_eround23	%f34,%f4,%f6,%f6
	.word	0x80c94408 !aes_eround01	%f36,%f8,%f2,%f0
	.word	0x84c9c428 !aes_eround23	%f38,%f8,%f2,%f2
	.word	0x88c94c0a !aes_eround01	%f36,%f10,%f6,%f4
	.word	0x8cc9cc2a !aes_eround23	%f38,%f10,%f6,%f6
	.word	0x90ca4400 !aes_eround01	%f40,%f0,%f2,%f8
	.word	0x84cac420 !aes_eround23	%f42,%f0,%f2,%f2
	.word	0x94ca4c04 !aes_eround01	%f40,%f4,%f6,%f10
	.word	0x8ccacc24 !aes_eround23	%f42,%f4,%f6,%f6
	.word	0x80cb4408 !aes_eround01	%f44,%f8,%f2,%f0
	.word	0x84cbc428 !aes_eround23	%f46,%f8,%f2,%f2
	.word	0x88cb4c0a !aes_eround01	%f44,%f10,%f6,%f4
	.word	0x8ccbcc2a !aes_eround23	%f46,%f10,%f6,%f6
	.word	0x90cc4400 !aes_eround01	%f48,%f0,%f2,%f8
	.word	0x84ccc420 !aes_eround23	%f50,%f0,%f2,%f2
	.word	0x94cc4c04 !aes_eround01	%f48,%f4,%f6,%f10
	.word	0x8ccccc24 !aes_eround23	%f50,%f4,%f6,%f6
	.word	0x80cd4408 !aes_eround01	%f52,%f8,%f2,%f0
	.word	0x84cdc428 !aes_eround23	%f54,%f8,%f2,%f2
	.word	0x88cd4c0a !aes_eround01	%f52,%f10,%f6,%f4
	.word	0x8ccdcc2a !aes_eround23	%f54,%f10,%f6,%f6
	.word	0x90ce4400 !aes_eround01	%f56,%f0,%f2,%f8
	.word	0x84cec420 !aes_eround23	%f58,%f0,%f2,%f2
	.word	0x94ce4c04 !aes_eround01	%f56,%f4,%f6,%f10
	.word	0x8ccecc24 !aes_eround23	%f58,%f4,%f6,%f6
	.word	0x80cf4488 !aes_eround01_l	%f60,%f8,%f2,%f0
	.word	0x84cfc4a8 !aes_eround23_l	%f62,%f8,%f2,%f2
	.word	0x88cf4c8a !aes_eround01_l	%f60,%f10,%f6,%f4
	retl
	.word	0x8ccfccaa !aes_eround23_l	%f62,%f10,%f6,%f6
.type	_aes192_encrypt_2x,#function
.size	_aes192_encrypt_2x,.-_aes192_encrypt_2x

.align	32
_aes256_encrypt_1x:
	.word	0x88cc0400 !aes_eround01	%f16,%f0,%f2,%f4
	.word	0x84cc8420 !aes_eround23	%f18,%f0,%f2,%f2
	ldd		[%i3 + 208], %f16
	ldd		[%i3 + 216], %f18
	.word	0x80cd0404 !aes_eround01	%f20,%f4,%f2,%f0
	.word	0x84cd8424 !aes_eround23	%f22,%f4,%f2,%f2
	ldd		[%i3 + 224], %f20
	ldd		[%i3 + 232], %f22
	.word	0x88ce0400 !aes_eround01	%f24,%f0,%f2,%f4
	.word	0x84ce8420 !aes_eround23	%f26,%f0,%f2,%f2
	.word	0x80cf0404 !aes_eround01	%f28,%f4,%f2,%f0
	.word	0x84cf8424 !aes_eround23	%f30,%f4,%f2,%f2
	.word	0x88c84400 !aes_eround01	%f32,%f0,%f2,%f4
	.word	0x84c8c420 !aes_eround23	%f34,%f0,%f2,%f2
	.word	0x80c94404 !aes_eround01	%f36,%f4,%f2,%f0
	.word	0x84c9c424 !aes_eround23	%f38,%f4,%f2,%f2
	.word	0x88ca4400 !aes_eround01	%f40,%f0,%f2,%f4
	.word	0x84cac420 !aes_eround23	%f42,%f0,%f2,%f2
	.word	0x80cb4404 !aes_eround01	%f44,%f4,%f2,%f0
	.word	0x84cbc424 !aes_eround23	%f46,%f4,%f2,%f2
	.word	0x88cc4400 !aes_eround01	%f48,%f0,%f2,%f4
	.word	0x84ccc420 !aes_eround23	%f50,%f0,%f2,%f2
	.word	0x80cd4404 !aes_eround01	%f52,%f4,%f2,%f0
	.word	0x84cdc424 !aes_eround23	%f54,%f4,%f2,%f2
	.word	0x88ce4400 !aes_eround01	%f56,%f0,%f2,%f4
	.word	0x84cec420 !aes_eround23	%f58,%f0,%f2,%f2
	.word	0x80cf4404 !aes_eround01	%f60,%f4,%f2,%f0
	.word	0x84cfc424 !aes_eround23	%f62,%f4,%f2,%f2
	.word	0x88cc0400 !aes_eround01	%f16,%f0,%f2,%f4
	.word	0x84cc8420 !aes_eround23	%f18,%f0,%f2,%f2
	ldd		[%i3 + 16], %f16
	ldd		[%i3 + 24], %f18
	.word	0x80cd0484 !aes_eround01_l	%f20,%f4,%f2,%f0
	.word	0x84cd84a4 !aes_eround23_l	%f22,%f4,%f2,%f2
	ldd		[%i3 + 32], %f20
	retl
	ldd		[%i3 + 40], %f22
.type	_aes256_encrypt_1x,#function
.size	_aes256_encrypt_1x,.-_aes256_encrypt_1x

.align	32
_aes256_encrypt_2x:
	.word	0x90cc0400 !aes_eround01	%f16,%f0,%f2,%f8
	.word	0x84cc8420 !aes_eround23	%f18,%f0,%f2,%f2
	.word	0x94cc0c04 !aes_eround01	%f16,%f4,%f6,%f10
	.word	0x8ccc8c24 !aes_eround23	%f18,%f4,%f6,%f6
	ldd		[%i3 + 208], %f16
	ldd		[%i3 + 216], %f18
	.word	0x80cd0408 !aes_eround01	%f20,%f8,%f2,%f0
	.word	0x84cd8428 !aes_eround23	%f22,%f8,%f2,%f2
	.word	0x88cd0c0a !aes_eround01	%f20,%f10,%f6,%f4
	.word	0x8ccd8c2a !aes_eround23	%f22,%f10,%f6,%f6
	ldd		[%i3 + 224], %f20
	ldd		[%i3 + 232], %f22
	.word	0x90ce0400 !aes_eround01	%f24,%f0,%f2,%f8
	.word	0x84ce8420 !aes_eround23	%f26,%f0,%f2,%f2
	.word	0x94ce0c04 !aes_eround01	%f24,%f4,%f6,%f10
	.word	0x8cce8c24 !aes_eround23	%f26,%f4,%f6,%f6
	.word	0x80cf0408 !aes_eround01	%f28,%f8,%f2,%f0
	.word	0x84cf8428 !aes_eround23	%f30,%f8,%f2,%f2
	.word	0x88cf0c0a !aes_eround01	%f28,%f10,%f6,%f4
	.word	0x8ccf8c2a !aes_eround23	%f30,%f10,%f6,%f6
	.word	0x90c84400 !aes_eround01	%f32,%f0,%f2,%f8
	.word	0x84c8c420 !aes_eround23	%f34,%f0,%f2,%f2
	.word	0x94c84c04 !aes_eround01	%f32,%f4,%f6,%f10
	.word	0x8cc8cc24 !aes_eround23	%f34,%f4,%f6,%f6
	.word	0x80c94408 !aes_eround01	%f36,%f8,%f2,%f0
	.word	0x84c9c428 !aes_eround23	%f38,%f8,%f2,%f2
	.word	0x88c94c0a !aes_eround01	%f36,%f10,%f6,%f4
	.word	0x8cc9cc2a !aes_eround23	%f38,%f10,%f6,%f6
	.word	0x90ca4400 !aes_eround01	%f40,%f0,%f2,%f8
	.word	0x84cac420 !aes_eround23	%f42,%f0,%f2,%f2
	.word	0x94ca4c04 !aes_eround01	%f40,%f4,%f6,%f10
	.word	0x8ccacc24 !aes_eround23	%f42,%f4,%f6,%f6
	.word	0x80cb4408 !aes_eround01	%f44,%f8,%f2,%f0
	.word	0x84cbc428 !aes_eround23	%f46,%f8,%f2,%f2
	.word	0x88cb4c0a !aes_eround01	%f44,%f10,%f6,%f4
	.word	0x8ccbcc2a !aes_eround23	%f46,%f10,%f6,%f6
	.word	0x90cc4400 !aes_eround01	%f48,%f0,%f2,%f8
	.word	0x84ccc420 !aes_eround23	%f50,%f0,%f2,%f2
	.word	0x94cc4c04 !aes_eround01	%f48,%f4,%f6,%f10
	.word	0x8ccccc24 !aes_eround23	%f50,%f4,%f6,%f6
	.word	0x80cd4408 !aes_eround01	%f52,%f8,%f2,%f0
	.word	0x84cdc428 !aes_eround23	%f54,%f8,%f2,%f2
	.word	0x88cd4c0a !aes_eround01	%f52,%f10,%f6,%f4
	.word	0x8ccdcc2a !aes_eround23	%f54,%f10,%f6,%f6
	.word	0x90ce4400 !aes_eround01	%f56,%f0,%f2,%f8
	.word	0x84cec420 !aes_eround23	%f58,%f0,%f2,%f2
	.word	0x94ce4c04 !aes_eround01	%f56,%f4,%f6,%f10
	.word	0x8ccecc24 !aes_eround23	%f58,%f4,%f6,%f6
	.word	0x80cf4408 !aes_eround01	%f60,%f8,%f2,%f0
	.word	0x84cfc428 !aes_eround23	%f62,%f8,%f2,%f2
	.word	0x88cf4c0a !aes_eround01	%f60,%f10,%f6,%f4
	.word	0x8ccfcc2a !aes_eround23	%f62,%f10,%f6,%f6
	.word	0x90cc0400 !aes_eround01	%f16,%f0,%f2,%f8
	.word	0x84cc8420 !aes_eround23	%f18,%f0,%f2,%f2
	.word	0x94cc0c04 !aes_eround01	%f16,%f4,%f6,%f10
	.word	0x8ccc8c24 !aes_eround23	%f18,%f4,%f6,%f6
	ldd		[%i3 + 16], %f16
	ldd		[%i3 + 24], %f18
	.word	0x80cd0488 !aes_eround01_l	%f20,%f8,%f2,%f0
	.word	0x84cd84a8 !aes_eround23_l	%f22,%f8,%f2,%f2
	.word	0x88cd0c8a !aes_eround01_l	%f20,%f10,%f6,%f4
	.word	0x8ccd8caa !aes_eround23_l	%f22,%f10,%f6,%f6
	ldd		[%i3 + 32], %f20
	retl
	ldd		[%i3 + 40], %f22
.type	_aes256_encrypt_2x,#function
.size	_aes256_encrypt_2x,.-_aes256_encrypt_2x

.align	32
_aes192_loadkey:
	ldx		[%i3 + 0], %g4
	ldx		[%i3 + 8], %g5
	ldd		[%i3 + 16], %f16
	ldd		[%i3 + 24], %f18
	ldd		[%i3 + 32], %f20
	ldd		[%i3 + 40], %f22
	ldd		[%i3 + 48], %f24
	ldd		[%i3 + 56], %f26
	ldd		[%i3 + 64], %f28
	ldd		[%i3 + 72], %f30
	ldd		[%i3 + 80], %f32
	ldd		[%i3 + 88], %f34
	ldd		[%i3 + 96], %f36
	ldd		[%i3 + 104], %f38
	ldd		[%i3 + 112], %f40
	ldd		[%i3 + 120], %f42
	ldd		[%i3 + 128], %f44
	ldd		[%i3 + 136], %f46
	ldd		[%i3 + 144], %f48
	ldd		[%i3 + 152], %f50
	ldd		[%i3 + 160], %f52
	ldd		[%i3 + 168], %f54
	ldd		[%i3 + 176], %f56
	ldd		[%i3 + 184], %f58
	ldd		[%i3 + 192], %f60
	ldd		[%i3 + 200], %f62
	retl
	nop
.type	_aes192_loadkey,#function
.size	_aes192_loadkey,.-_aes192_loadkey
_aes256_loadkey=_aes192_loadkey
_aes192_load_enckey=_aes192_loadkey
_aes192_load_deckey=_aes192_loadkey
_aes256_load_enckey=_aes192_loadkey
_aes256_load_deckey=_aes192_loadkey
.globl	aes256_t4_cbc_encrypt
.align	32
aes256_t4_cbc_encrypt:
	save		%sp, -192, %sp
	cmp		%i2, 0
	be,pn		%xcc, .L256_cbc_enc_abort
	srln		%i2, 0, %i2		! needed on v8+, "nop" on v9
	sub		%i0, %i1, %l5	! %i0!=%i1
	ld		[%i4 + 0], %f0
	ld		[%i4 + 4], %f1
	ld		[%i4 + 8], %f2
	ld		[%i4 + 12], %f3
	prefetch	[%i0], 20
	prefetch	[%i0 + 63], 20
	call		_aes256_load_enckey
	and		%i0, 7, %l0
	andn		%i0, 7, %i0
	sll		%l0, 3, %l0
	mov		64, %l1
	mov		0xff, %l3
	sub		%l1, %l0, %l1
	and		%i1, 7, %l2
	cmp		%i2, 127
	movrnz		%l2, 0, %l5		! if (	%i1&7 ||
	movleu		%xcc, 0, %l5	!	%i2<128 ||
	brnz,pn		%l5, .L256cbc_enc_blk	!	%i0==%i1)
	srl		%l3, %l2, %l3

	.word	0xb3b64340 !alignaddrl	%i1,%g0,%i1
	srlx		%i2, 4, %i2
	prefetch	[%i1], 22

.L256_cbc_enc_loop:
	ldx		[%i0 + 0], %o0
	brz,pt		%l0, 4f
	ldx		[%i0 + 8], %o1

	ldx		[%i0 + 16], %o2
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	sllx		%o1, %l0, %o1
	or		%g1, %o0, %o0
	srlx		%o2, %l1, %o2
	or		%o2, %o1, %o1
4:
	xor		%g4, %o0, %o0		! ^= rk[0]
	xor		%g5, %o1, %o1
	.word	0x99b02308 !movxtod	%o0,%f12
	.word	0x9db02309 !movxtod	%o1,%f14

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= ivec
	.word	0x85b38d82 !fxor	%f14,%f2,%f2
	prefetch	[%i1 + 63], 22
	prefetch	[%i0 + 16+63], 20
	call		_aes256_encrypt_1x
	add		%i0, 16, %i0

	brnz,pn		%l2, 2f
	sub		%i2, 1, %i2
		
	std		%f0, [%i1 + 0]
	std		%f2, [%i1 + 8]
	brnz,pt		%i2, .L256_cbc_enc_loop
	add		%i1, 16, %i1
	st		%f0, [%i4 + 0]
	st		%f1, [%i4 + 4]
	st		%f2, [%i4 + 8]
	st		%f3, [%i4 + 12]
.L256_cbc_enc_abort:
	ret
	restore

.align	16
2:	ldxa		[%i0]0x82, %o0		! avoid read-after-write hazard
						! and ~3x deterioration
						! in inp==out case
	.word	0x89b00900 !faligndata	%f0,%f0,%f4		! handle unaligned output
	.word	0x8db00902 !faligndata	%f0,%f2,%f6
	.word	0x91b08902 !faligndata	%f2,%f2,%f8

	stda		%f4, [%i1 + %l3]0xc0	! partial store
	std		%f6, [%i1 + 8]
	add		%i1, 16, %i1
	orn		%g0, %l3, %l3
	stda		%f8, [%i1 + %l3]0xc0	! partial store

	brnz,pt		%i2, .L256_cbc_enc_loop+4
	orn		%g0, %l3, %l3
	st		%f0, [%i4 + 0]
	st		%f1, [%i4 + 4]
	st		%f2, [%i4 + 8]
	st		%f3, [%i4 + 12]
	ret
	restore

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
.align	32
.L256cbc_enc_blk:
	add	%i1, %i2, %l5
	and	%l5, 63, %l5	! tail
	sub	%i2, %l5, %i2
	add	%l5, 15, %l5	! round up to 16n
	srlx	%i2, 4, %i2
	srl	%l5, 4, %l5

.L256_cbc_enc_blk_loop:
	ldx		[%i0 + 0], %o0
	brz,pt		%l0, 5f
	ldx		[%i0 + 8], %o1

	ldx		[%i0 + 16], %o2
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	sllx		%o1, %l0, %o1
	or		%g1, %o0, %o0
	srlx		%o2, %l1, %o2
	or		%o2, %o1, %o1
5:
	xor		%g4, %o0, %o0		! ^= rk[0]
	xor		%g5, %o1, %o1
	.word	0x99b02308 !movxtod	%o0,%f12
	.word	0x9db02309 !movxtod	%o1,%f14

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= ivec
	.word	0x85b38d82 !fxor	%f14,%f2,%f2
	prefetch	[%i0 + 16+63], 20
	call		_aes256_encrypt_1x
	add		%i0, 16, %i0
	sub		%i2, 1, %i2
		
	stda		%f0, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	add		%i1, 8, %i1
	stda		%f2, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	brnz,pt		%i2, .L256_cbc_enc_blk_loop
	add		%i1, 8, %i1

	membar		#StoreLoad|#StoreStore
	brnz,pt		%l5, .L256_cbc_enc_loop
	mov		%l5, %i2
	st		%f0, [%i4 + 0]
	st		%f1, [%i4 + 4]
	st		%f2, [%i4 + 8]
	st		%f3, [%i4 + 12]
	ret
	restore
.type	aes256_t4_cbc_encrypt,#function
.size	aes256_t4_cbc_encrypt,.-aes256_t4_cbc_encrypt
.globl	aes192_t4_cbc_encrypt
.align	32
aes192_t4_cbc_encrypt:
	save		%sp, -192, %sp
	cmp		%i2, 0
	be,pn		%xcc, .L192_cbc_enc_abort
	srln		%i2, 0, %i2		! needed on v8+, "nop" on v9
	sub		%i0, %i1, %l5	! %i0!=%i1
	ld		[%i4 + 0], %f0
	ld		[%i4 + 4], %f1
	ld		[%i4 + 8], %f2
	ld		[%i4 + 12], %f3
	prefetch	[%i0], 20
	prefetch	[%i0 + 63], 20
	call		_aes192_load_enckey
	and		%i0, 7, %l0
	andn		%i0, 7, %i0
	sll		%l0, 3, %l0
	mov		64, %l1
	mov		0xff, %l3
	sub		%l1, %l0, %l1
	and		%i1, 7, %l2
	cmp		%i2, 127
	movrnz		%l2, 0, %l5		! if (	%i1&7 ||
	movleu		%xcc, 0, %l5	!	%i2<128 ||
	brnz,pn		%l5, .L192cbc_enc_blk	!	%i0==%i1)
	srl		%l3, %l2, %l3

	.word	0xb3b64340 !alignaddrl	%i1,%g0,%i1
	srlx		%i2, 4, %i2
	prefetch	[%i1], 22

.L192_cbc_enc_loop:
	ldx		[%i0 + 0], %o0
	brz,pt		%l0, 4f
	ldx		[%i0 + 8], %o1

	ldx		[%i0 + 16], %o2
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	sllx		%o1, %l0, %o1
	or		%g1, %o0, %o0
	srlx		%o2, %l1, %o2
	or		%o2, %o1, %o1
4:
	xor		%g4, %o0, %o0		! ^= rk[0]
	xor		%g5, %o1, %o1
	.word	0x99b02308 !movxtod	%o0,%f12
	.word	0x9db02309 !movxtod	%o1,%f14

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= ivec
	.word	0x85b38d82 !fxor	%f14,%f2,%f2
	prefetch	[%i1 + 63], 22
	prefetch	[%i0 + 16+63], 20
	call		_aes192_encrypt_1x
	add		%i0, 16, %i0

	brnz,pn		%l2, 2f
	sub		%i2, 1, %i2
		
	std		%f0, [%i1 + 0]
	std		%f2, [%i1 + 8]
	brnz,pt		%i2, .L192_cbc_enc_loop
	add		%i1, 16, %i1
	st		%f0, [%i4 + 0]
	st		%f1, [%i4 + 4]
	st		%f2, [%i4 + 8]
	st		%f3, [%i4 + 12]
.L192_cbc_enc_abort:
	ret
	restore

.align	16
2:	ldxa		[%i0]0x82, %o0		! avoid read-after-write hazard
						! and ~3x deterioration
						! in inp==out case
	.word	0x89b00900 !faligndata	%f0,%f0,%f4		! handle unaligned output
	.word	0x8db00902 !faligndata	%f0,%f2,%f6
	.word	0x91b08902 !faligndata	%f2,%f2,%f8

	stda		%f4, [%i1 + %l3]0xc0	! partial store
	std		%f6, [%i1 + 8]
	add		%i1, 16, %i1
	orn		%g0, %l3, %l3
	stda		%f8, [%i1 + %l3]0xc0	! partial store

	brnz,pt		%i2, .L192_cbc_enc_loop+4
	orn		%g0, %l3, %l3
	st		%f0, [%i4 + 0]
	st		%f1, [%i4 + 4]
	st		%f2, [%i4 + 8]
	st		%f3, [%i4 + 12]
	ret
	restore

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
.align	32
.L192cbc_enc_blk:
	add	%i1, %i2, %l5
	and	%l5, 63, %l5	! tail
	sub	%i2, %l5, %i2
	add	%l5, 15, %l5	! round up to 16n
	srlx	%i2, 4, %i2
	srl	%l5, 4, %l5

.L192_cbc_enc_blk_loop:
	ldx		[%i0 + 0], %o0
	brz,pt		%l0, 5f
	ldx		[%i0 + 8], %o1

	ldx		[%i0 + 16], %o2
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	sllx		%o1, %l0, %o1
	or		%g1, %o0, %o0
	srlx		%o2, %l1, %o2
	or		%o2, %o1, %o1
5:
	xor		%g4, %o0, %o0		! ^= rk[0]
	xor		%g5, %o1, %o1
	.word	0x99b02308 !movxtod	%o0,%f12
	.word	0x9db02309 !movxtod	%o1,%f14

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= ivec
	.word	0x85b38d82 !fxor	%f14,%f2,%f2
	prefetch	[%i0 + 16+63], 20
	call		_aes192_encrypt_1x
	add		%i0, 16, %i0
	sub		%i2, 1, %i2
		
	stda		%f0, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	add		%i1, 8, %i1
	stda		%f2, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	brnz,pt		%i2, .L192_cbc_enc_blk_loop
	add		%i1, 8, %i1

	membar		#StoreLoad|#StoreStore
	brnz,pt		%l5, .L192_cbc_enc_loop
	mov		%l5, %i2
	st		%f0, [%i4 + 0]
	st		%f1, [%i4 + 4]
	st		%f2, [%i4 + 8]
	st		%f3, [%i4 + 12]
	ret
	restore
.type	aes192_t4_cbc_encrypt,#function
.size	aes192_t4_cbc_encrypt,.-aes192_t4_cbc_encrypt
.globl	aes256_t4_ctr32_encrypt
.align	32
aes256_t4_ctr32_encrypt:
	save		%sp, -192, %sp
	srln		%i2, 0, %i2		! needed on v8+, "nop" on v9

	prefetch	[%i0], 20
	prefetch	[%i0 + 63], 20
	call		_aes256_load_enckey
	sllx		%i2, 4, %i2

	ld		[%i4 + 0], %l4	! counter
	ld		[%i4 + 4], %l5
	ld		[%i4 + 8], %l6
	ld		[%i4 + 12], %l7

	sllx		%l4, 32, %o5
	or		%l5, %o5, %o5
	sllx		%l6, 32, %g1
	xor		%o5, %g4, %g4		! ^= rk[0]
	xor		%g1, %g5, %g5
	.word	0x9db02304 !movxtod	%g4,%f14		! most significant 64 bits

	sub		%i0, %i1, %l5	! %i0!=%i1
	and		%i0, 7, %l0
	andn		%i0, 7, %i0
	sll		%l0, 3, %l0
	mov		64, %l1
	mov		0xff, %l3
	sub		%l1, %l0, %l1
	and		%i1, 7, %l2
	cmp		%i2, 255
	movrnz		%l2, 0, %l5		! if (	%i1&7 ||
	movleu		%xcc, 0, %l5	!	%i2<256 ||
	brnz,pn		%l5, .L256_ctr32_blk	!	%i0==%i1)
	srl		%l3, %l2, %l3

	andcc		%i2, 16, %g0		! is number of blocks even?
	.word	0xb3b64340 !alignaddrl	%i1,%g0,%i1
	bz		%icc, .L256_ctr32_loop2x
	srlx		%i2, 4, %i2
.L256_ctr32_loop:
	ldx		[%i0 + 0], %o0
	brz,pt		%l0, 4f
	ldx		[%i0 + 8], %o1

	ldx		[%i0 + 16], %o2
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	sllx		%o1, %l0, %o1
	or		%g1, %o0, %o0
	srlx		%o2, %l1, %o2
	or		%o2, %o1, %o1
4:
	xor		%g5, %l7, %g1		! ^= rk[0]
	add		%l7, 1, %l7
	.word	0x85b02301 !movxtod	%g1,%f2
	srl		%l7, 0, %l7		! clruw
	prefetch	[%i1 + 63], 22
	prefetch	[%i0 + 16+63], 20
	.word	0x88cc040e !aes_eround01	%f16,%f14,%f2,%f4
	.word	0x84cc842e !aes_eround23	%f18,%f14,%f2,%f2
	call		_aes256_encrypt_1x+8
	add		%i0, 16, %i0

	.word	0x95b02308 !movxtod	%o0,%f10
	.word	0x99b02309 !movxtod	%o1,%f12
	.word	0x81b28d80 !fxor	%f10,%f0,%f0		! ^= inp
	.word	0x85b30d82 !fxor	%f12,%f2,%f2

	brnz,pn		%l2, 2f
	sub		%i2, 1, %i2
		
	std		%f0, [%i1 + 0]
	std		%f2, [%i1 + 8]
	brnz,pt		%i2, .L256_ctr32_loop2x
	add		%i1, 16, %i1

	ret
	restore

.align	16
2:	ldxa		[%i0]0x82, %o0		! avoid read-after-write hazard
						! and ~3x deterioration
						! in inp==out case
	.word	0x89b00900 !faligndata	%f0,%f0,%f4		! handle unaligned output
	.word	0x8db00902 !faligndata	%f0,%f2,%f6
	.word	0x91b08902 !faligndata	%f2,%f2,%f8
	stda		%f4, [%i1 + %l3]0xc0	! partial store
	std		%f6, [%i1 + 8]
	add		%i1, 16, %i1
	orn		%g0, %l3, %l3
	stda		%f8, [%i1 + %l3]0xc0	! partial store

	brnz,pt		%i2, .L256_ctr32_loop2x+4
	orn		%g0, %l3, %l3

	ret
	restore

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
.align	32
.L256_ctr32_loop2x:
	ldx		[%i0 + 0], %o0
	ldx		[%i0 + 8], %o1
	ldx		[%i0 + 16], %o2
	brz,pt		%l0, 4f
	ldx		[%i0 + 24], %o3

	ldx		[%i0 + 32], %o4
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	or		%g1, %o0, %o0
	sllx		%o1, %l0, %o1
	srlx		%o2, %l1, %g1
	or		%g1, %o1, %o1
	sllx		%o2, %l0, %o2
	srlx		%o3, %l1, %g1
	or		%g1, %o2, %o2
	sllx		%o3, %l0, %o3
	srlx		%o4, %l1, %o4
	or		%o4, %o3, %o3
4:
	xor		%g5, %l7, %g1		! ^= rk[0]
	add		%l7, 1, %l7
	.word	0x85b02301 !movxtod	%g1,%f2
	srl		%l7, 0, %l7		! clruw
	xor		%g5, %l7, %g1
	add		%l7, 1, %l7
	.word	0x8db02301 !movxtod	%g1,%f6
	srl		%l7, 0, %l7		! clruw
	prefetch	[%i1 + 63], 22
	prefetch	[%i0 + 32+63], 20
	.word	0x90cc040e !aes_eround01	%f16,%f14,%f2,%f8
	.word	0x84cc842e !aes_eround23	%f18,%f14,%f2,%f2
	.word	0x94cc0c0e !aes_eround01	%f16,%f14,%f6,%f10
	.word	0x8ccc8c2e !aes_eround23	%f18,%f14,%f6,%f6
	call		_aes256_encrypt_2x+16
	add		%i0, 32, %i0

	.word	0x91b02308 !movxtod	%o0,%f8
	.word	0x95b02309 !movxtod	%o1,%f10
	.word	0x99b0230a !movxtod	%o2,%f12
	.word	0x81b20d80 !fxor	%f8,%f0,%f0		! ^= inp
	.word	0x91b0230b !movxtod	%o3,%f8
	.word	0x85b28d82 !fxor	%f10,%f2,%f2
	.word	0x89b30d84 !fxor	%f12,%f4,%f4
	.word	0x8db20d86 !fxor	%f8,%f6,%f6

	brnz,pn		%l2, 2f
	sub		%i2, 2, %i2
		
	std		%f0, [%i1 + 0]
	std		%f2, [%i1 + 8]
	std		%f4, [%i1 + 16]
	std		%f6, [%i1 + 24]
	brnz,pt		%i2, .L256_ctr32_loop2x
	add		%i1, 32, %i1

	ret
	restore

.align	16
2:	ldxa		[%i0]0x82, %o0		! avoid read-after-write hazard
						! and ~3x deterioration
						! in inp==out case
	.word	0x91b00900 !faligndata	%f0,%f0,%f8		! handle unaligned output
	.word	0x81b00902 !faligndata	%f0,%f2,%f0
	.word	0x85b08904 !faligndata	%f2,%f4,%f2
	.word	0x89b10906 !faligndata	%f4,%f6,%f4
	.word	0x8db18906 !faligndata	%f6,%f6,%f6

	stda		%f8, [%i1 + %l3]0xc0	! partial store
	std		%f0, [%i1 + 8]
	std		%f2, [%i1 + 16]
	std		%f4, [%i1 + 24]
	add		%i1, 32, %i1
	orn		%g0, %l3, %l3
	stda		%f6, [%i1 + %l3]0xc0	! partial store

	brnz,pt		%i2, .L256_ctr32_loop2x+4
	orn		%g0, %l3, %l3

	ret
	restore

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
.align	32
.L256_ctr32_blk:
	add	%i1, %i2, %l5
	and	%l5, 63, %l5	! tail
	sub	%i2, %l5, %i2
	add	%l5, 15, %l5	! round up to 16n
	srlx	%i2, 4, %i2
	srl	%l5, 4, %l5
	sub	%i2, 1, %i2
	add	%l5, 1, %l5

.L256_ctr32_blk_loop2x:
	ldx		[%i0 + 0], %o0
	ldx		[%i0 + 8], %o1
	ldx		[%i0 + 16], %o2
	brz,pt		%l0, 5f
	ldx		[%i0 + 24], %o3

	ldx		[%i0 + 32], %o4
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	or		%g1, %o0, %o0
	sllx		%o1, %l0, %o1
	srlx		%o2, %l1, %g1
	or		%g1, %o1, %o1
	sllx		%o2, %l0, %o2
	srlx		%o3, %l1, %g1
	or		%g1, %o2, %o2
	sllx		%o3, %l0, %o3
	srlx		%o4, %l1, %o4
	or		%o4, %o3, %o3
5:
	xor		%g5, %l7, %g1		! ^= rk[0]
	add		%l7, 1, %l7
	.word	0x85b02301 !movxtod	%g1,%f2
	srl		%l7, 0, %l7		! clruw
	xor		%g5, %l7, %g1
	add		%l7, 1, %l7
	.word	0x8db02301 !movxtod	%g1,%f6
	srl		%l7, 0, %l7		! clruw
	prefetch	[%i0 + 32+63], 20
	.word	0x90cc040e !aes_eround01	%f16,%f14,%f2,%f8
	.word	0x84cc842e !aes_eround23	%f18,%f14,%f2,%f2
	.word	0x94cc0c0e !aes_eround01	%f16,%f14,%f6,%f10
	.word	0x8ccc8c2e !aes_eround23	%f18,%f14,%f6,%f6
	call		_aes256_encrypt_2x+16
	add		%i0, 32, %i0
	subcc		%i2, 2, %i2

	.word	0x91b02308 !movxtod	%o0,%f8
	.word	0x95b02309 !movxtod	%o1,%f10
	.word	0x99b0230a !movxtod	%o2,%f12
	.word	0x81b20d80 !fxor	%f8,%f0,%f0		! ^= inp
	.word	0x91b0230b !movxtod	%o3,%f8
	.word	0x85b28d82 !fxor	%f10,%f2,%f2
	.word	0x89b30d84 !fxor	%f12,%f4,%f4
	.word	0x8db20d86 !fxor	%f8,%f6,%f6

	stda		%f0, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	add		%i1, 8, %i1
	stda		%f2, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	add		%i1, 8, %i1
	stda		%f4, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	add		%i1, 8, %i1
	stda		%f6, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	bgu,pt		%xcc, .L256_ctr32_blk_loop2x
	add		%i1, 8, %i1

	add		%l5, %i2, %i2
	andcc		%i2, 1, %g0		! is number of blocks even?
	membar		#StoreLoad|#StoreStore
	bnz,pt		%icc, .L256_ctr32_loop
	srl		%i2, 0, %i2
	brnz,pn		%i2, .L256_ctr32_loop2x
	nop

	ret
	restore
.type	aes256_t4_ctr32_encrypt,#function
.size	aes256_t4_ctr32_encrypt,.-aes256_t4_ctr32_encrypt
.globl	aes256_t4_xts_encrypt
.align	32
aes256_t4_xts_encrypt:
	save		%sp, -192-16, %sp
	srln		%i2, 0, %i2		! needed on v8+, "nop" on v9

	mov		%i5, %o0
	add		%fp, 2047-16, %o1
	call		aes_t4_encrypt
	mov		%i4, %o2

	add		%fp, 2047-16, %l7
	ldxa		[%l7]0x88, %g2
	add		%fp, 2047-8, %l7
	ldxa		[%l7]0x88, %g3		! %g3:%g2 is tweak

	sethi		%hi(0x76543210), %l7
	or		%l7, %lo(0x76543210), %l7
	.word	0x81b5c320 !bmask	%l7,%g0,%g0		! byte swap mask

	prefetch	[%i0], 20
	prefetch	[%i0 + 63], 20
	call		_aes256_load_enckey
	and		%i2, 15,  %i5
	and		%i2, -16, %i2

	sub		%i0, %i1, %l5	! %i0!=%i1
	and		%i0, 7, %l0
	andn		%i0, 7, %i0
	sll		%l0, 3, %l0
	mov		64, %l1
	mov		0xff, %l3
	sub		%l1, %l0, %l1
	and		%i1, 7, %l2
	cmp		%i2, 255
	movrnz		%l2, 0, %l5		! if (	%i1&7 ||
	movleu		%xcc, 0, %l5	!	%i2<256 ||
	brnz,pn		%l5, .L256_xts_enblk !	%i0==%i1)
	srl		%l3, %l2, %l3

	andcc		%i2, 16, %g0		! is number of blocks even?
	.word	0xb3b64340 !alignaddrl	%i1,%g0,%i1
	bz		%icc, .L256_xts_enloop2x
	srlx		%i2, 4, %i2
.L256_xts_enloop:
	ldx		[%i0 + 0], %o0
	brz,pt		%l0, 4f
	ldx		[%i0 + 8], %o1

	ldx		[%i0 + 16], %o2
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	sllx		%o1, %l0, %o1
	or		%g1, %o0, %o0
	srlx		%o2, %l1, %o2
	or		%o2, %o1, %o1
4:
	.word	0x99b02302 !movxtod	%g2,%f12
	.word	0x9db02303 !movxtod	%g3,%f14
	.word	0x99b3098c !bshuffle	%f12,%f12,%f12
	.word	0x9db3898e !bshuffle	%f14,%f14,%f14

	xor		%g4, %o0, %o0		! ^= rk[0]
	xor		%g5, %o1, %o1
	.word	0x81b02308 !movxtod	%o0,%f0
	.word	0x85b02309 !movxtod	%o1,%f2

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= tweak[0]
	.word	0x85b38d82 !fxor	%f14,%f2,%f2

	prefetch	[%i1 + 63], 22
	prefetch	[%i0 + 16+63], 20
	call		_aes256_encrypt_1x
	add		%i0, 16, %i0

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= tweak[0]
	.word	0x85b38d82 !fxor	%f14,%f2,%f2

	srax		%g3, 63, %l7		! next tweak value
	addcc		%g2, %g2, %g2
	and		%l7, 0x87, %l7
	.word	0x87b0c223 !addxc	%g3,%g3,%g3
	xor		%l7, %g2, %g2

	brnz,pn		%l2, 2f
	sub		%i2, 1, %i2
		
	std		%f0, [%i1 + 0]
	std		%f2, [%i1 + 8]
	brnz,pt		%i2, .L256_xts_enloop2x
	add		%i1, 16, %i1

	brnz,pn		%i5, .L256_xts_ensteal
	nop

	ret
	restore

.align	16
2:	ldxa		[%i0]0x82, %o0		! avoid read-after-write hazard
						! and ~3x deterioration
						! in inp==out case
	.word	0x89b00900 !faligndata	%f0,%f0,%f4		! handle unaligned output
	.word	0x8db00902 !faligndata	%f0,%f2,%f6
	.word	0x91b08902 !faligndata	%f2,%f2,%f8
	stda		%f4, [%i1 + %l3]0xc0	! partial store
	std		%f6, [%i1 + 8]
	add		%i1, 16, %i1
	orn		%g0, %l3, %l3
	stda		%f8, [%i1 + %l3]0xc0	! partial store

	brnz,pt		%i2, .L256_xts_enloop2x+4
	orn		%g0, %l3, %l3

	brnz,pn		%i5, .L256_xts_ensteal
	nop

	ret
	restore

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
.align	32
.L256_xts_enloop2x:
	ldx		[%i0 + 0], %o0
	ldx		[%i0 + 8], %o1
	ldx		[%i0 + 16], %o2
	brz,pt		%l0, 4f
	ldx		[%i0 + 24], %o3

	ldx		[%i0 + 32], %o4
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	or		%g1, %o0, %o0
	sllx		%o1, %l0, %o1
	srlx		%o2, %l1, %g1
	or		%g1, %o1, %o1
	sllx		%o2, %l0, %o2
	srlx		%o3, %l1, %g1
	or		%g1, %o2, %o2
	sllx		%o3, %l0, %o3
	srlx		%o4, %l1, %o4
	or		%o4, %o3, %o3
4:
	.word	0x99b02302 !movxtod	%g2,%f12
	.word	0x9db02303 !movxtod	%g3,%f14
	.word	0x99b3098c !bshuffle	%f12,%f12,%f12
	.word	0x9db3898e !bshuffle	%f14,%f14,%f14

	srax		%g3, 63, %l7		! next tweak value
	addcc		%g2, %g2, %g2
	and		%l7, 0x87, %l7
	.word	0x87b0c223 !addxc	%g3,%g3,%g3
	xor		%l7, %g2, %g2

	.word	0x91b02302 !movxtod	%g2,%f8
	.word	0x95b02303 !movxtod	%g3,%f10
	.word	0x91b20988 !bshuffle	%f8,%f8,%f8
	.word	0x95b2898a !bshuffle	%f10,%f10,%f10

	xor		%g4, %o0, %o0		! ^= rk[0]
	xor		%g5, %o1, %o1
	xor		%g4, %o2, %o2		! ^= rk[0]
	xor		%g5, %o3, %o3
	.word	0x81b02308 !movxtod	%o0,%f0
	.word	0x85b02309 !movxtod	%o1,%f2
	.word	0x89b0230a !movxtod	%o2,%f4
	.word	0x8db0230b !movxtod	%o3,%f6

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= tweak[0]
	.word	0x85b38d82 !fxor	%f14,%f2,%f2
	.word	0x89b20d84 !fxor	%f8,%f4,%f4		! ^= tweak[0]
	.word	0x8db28d86 !fxor	%f10,%f6,%f6

	prefetch	[%i1 + 63], 22
	prefetch	[%i0 + 32+63], 20
	call		_aes256_encrypt_2x
	add		%i0, 32, %i0

	.word	0x91b02302 !movxtod	%g2,%f8
	.word	0x95b02303 !movxtod	%g3,%f10

	srax		%g3, 63, %l7		! next tweak value
	addcc		%g2, %g2, %g2
	and		%l7, 0x87, %l7
	.word	0x87b0c223 !addxc	%g3,%g3,%g3
	xor		%l7, %g2, %g2

	.word	0x91b20988 !bshuffle	%f8,%f8,%f8
	.word	0x95b2898a !bshuffle	%f10,%f10,%f10

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= tweak[0]
	.word	0x85b38d82 !fxor	%f14,%f2,%f2
	.word	0x89b20d84 !fxor	%f8,%f4,%f4
	.word	0x8db28d86 !fxor	%f10,%f6,%f6

	brnz,pn		%l2, 2f
	sub		%i2, 2, %i2
		
	std		%f0, [%i1 + 0]
	std		%f2, [%i1 + 8]
	std		%f4, [%i1 + 16]
	std		%f6, [%i1 + 24]
	brnz,pt		%i2, .L256_xts_enloop2x
	add		%i1, 32, %i1

	.word	0x81b00f04 !fsrc2	%f0,%f4,%f0
	.word	0x85b00f06 !fsrc2	%f0,%f6,%f2
	brnz,pn		%i5, .L256_xts_ensteal
	nop

	ret
	restore

.align	16
2:	ldxa		[%i0]0x82, %o0		! avoid read-after-write hazard
						! and ~3x deterioration
						! in inp==out case
	.word	0x91b00900 !faligndata	%f0,%f0,%f8		! handle unaligned output
	.word	0x95b00902 !faligndata	%f0,%f2,%f10
	.word	0x99b08904 !faligndata	%f2,%f4,%f12
	.word	0x9db10906 !faligndata	%f4,%f6,%f14
	.word	0x81b18906 !faligndata	%f6,%f6,%f0

	stda		%f8, [%i1 + %l3]0xc0	! partial store
	std		%f10, [%i1 + 8]
	std		%f12, [%i1 + 16]
	std		%f14, [%i1 + 24]
	add		%i1, 32, %i1
	orn		%g0, %l3, %l3
	stda		%f0, [%i1 + %l3]0xc0	! partial store

	brnz,pt		%i2, .L256_xts_enloop2x+4
	orn		%g0, %l3, %l3

	.word	0x81b00f04 !fsrc2	%f0,%f4,%f0
	.word	0x85b00f06 !fsrc2	%f0,%f6,%f2
	brnz,pn		%i5, .L256_xts_ensteal
	nop

	ret
	restore

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
.align	32
.L256_xts_enblk:
	add	%i1, %i2, %l5
	and	%l5, 63, %l5	! tail
	sub	%i2, %l5, %i2
	add	%l5, 15, %l5	! round up to 16n
	srlx	%i2, 4, %i2
	srl	%l5, 4, %l5
	sub	%i2, 1, %i2
	add	%l5, 1, %l5

.L256_xts_enblk2x:
	ldx		[%i0 + 0], %o0
	ldx		[%i0 + 8], %o1
	ldx		[%i0 + 16], %o2
	brz,pt		%l0, 5f
	ldx		[%i0 + 24], %o3

	ldx		[%i0 + 32], %o4
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	or		%g1, %o0, %o0
	sllx		%o1, %l0, %o1
	srlx		%o2, %l1, %g1
	or		%g1, %o1, %o1
	sllx		%o2, %l0, %o2
	srlx		%o3, %l1, %g1
	or		%g1, %o2, %o2
	sllx		%o3, %l0, %o3
	srlx		%o4, %l1, %o4
	or		%o4, %o3, %o3
5:
	.word	0x99b02302 !movxtod	%g2,%f12
	.word	0x9db02303 !movxtod	%g3,%f14
	.word	0x99b3098c !bshuffle	%f12,%f12,%f12
	.word	0x9db3898e !bshuffle	%f14,%f14,%f14

	srax		%g3, 63, %l7		! next tweak value
	addcc		%g2, %g2, %g2
	and		%l7, 0x87, %l7
	.word	0x87b0c223 !addxc	%g3,%g3,%g3
	xor		%l7, %g2, %g2

	.word	0x91b02302 !movxtod	%g2,%f8
	.word	0x95b02303 !movxtod	%g3,%f10
	.word	0x91b20988 !bshuffle	%f8,%f8,%f8
	.word	0x95b2898a !bshuffle	%f10,%f10,%f10

	xor		%g4, %o0, %o0		! ^= rk[0]
	xor		%g5, %o1, %o1
	xor		%g4, %o2, %o2		! ^= rk[0]
	xor		%g5, %o3, %o3
	.word	0x81b02308 !movxtod	%o0,%f0
	.word	0x85b02309 !movxtod	%o1,%f2
	.word	0x89b0230a !movxtod	%o2,%f4
	.word	0x8db0230b !movxtod	%o3,%f6

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= tweak[0]
	.word	0x85b38d82 !fxor	%f14,%f2,%f2
	.word	0x89b20d84 !fxor	%f8,%f4,%f4		! ^= tweak[0]
	.word	0x8db28d86 !fxor	%f10,%f6,%f6

	prefetch	[%i0 + 32+63], 20
	call		_aes256_encrypt_2x
	add		%i0, 32, %i0

	.word	0x91b02302 !movxtod	%g2,%f8
	.word	0x95b02303 !movxtod	%g3,%f10

	srax		%g3, 63, %l7		! next tweak value
	addcc		%g2, %g2, %g2
	and		%l7, 0x87, %l7
	.word	0x87b0c223 !addxc	%g3,%g3,%g3
	xor		%l7, %g2, %g2

	.word	0x91b20988 !bshuffle	%f8,%f8,%f8
	.word	0x95b2898a !bshuffle	%f10,%f10,%f10

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= tweak[0]
	.word	0x85b38d82 !fxor	%f14,%f2,%f2
	.word	0x89b20d84 !fxor	%f8,%f4,%f4
	.word	0x8db28d86 !fxor	%f10,%f6,%f6

	subcc		%i2, 2, %i2
	stda		%f0, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	add		%i1, 8, %i1
	stda		%f2, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	add		%i1, 8, %i1
	stda		%f4, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	add		%i1, 8, %i1
	stda		%f6, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	bgu,pt		%xcc, .L256_xts_enblk2x
	add		%i1, 8, %i1

	add		%l5, %i2, %i2
	andcc		%i2, 1, %g0		! is number of blocks even?
	membar		#StoreLoad|#StoreStore
	bnz,pt		%icc, .L256_xts_enloop
	srl		%i2, 0, %i2
	brnz,pn		%i2, .L256_xts_enloop2x
	nop

	.word	0x81b00f04 !fsrc2	%f0,%f4,%f0
	.word	0x85b00f06 !fsrc2	%f0,%f6,%f2
	brnz,pn		%i5, .L256_xts_ensteal
	nop

	ret
	restore
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
.align	32
.L256_xts_ensteal:
	std		%f0, [%fp + 2047-16]	! copy of output
	std		%f2, [%fp + 2047-8]

	srl		%l0, 3, %l0
	add		%fp, 2047-16, %l7
	add		%i0, %l0, %i0	! original %i0+%i2&-15
	add		%i1, %l2, %i1	! original %i1+%i2&-15
	mov		0, %l0
	nop					! align

.L256_xts_enstealing:
	ldub		[%i0 + %l0], %o0
	ldub		[%l7  + %l0], %o1
	dec		%i5
	stb		%o0, [%l7  + %l0]
	stb		%o1, [%i1 + %l0]
	brnz		%i5, .L256_xts_enstealing
	inc		%l0

	mov		%l7, %i0
	sub		%i1, 16, %i1
	mov		0, %l0
	sub		%i1, %l2, %i1
	ba		.L256_xts_enloop	! one more time
	mov		1, %i2				! %i5 is 0
	ret
	restore
.type	aes256_t4_xts_encrypt,#function
.size	aes256_t4_xts_encrypt,.-aes256_t4_xts_encrypt
.globl	aes256_t4_xts_decrypt
.align	32
aes256_t4_xts_decrypt:
	save		%sp, -192-16, %sp
	srln		%i2, 0, %i2		! needed on v8+, "nop" on v9

	mov		%i5, %o0
	add		%fp, 2047-16, %o1
	call		aes_t4_encrypt
	mov		%i4, %o2

	add		%fp, 2047-16, %l7
	ldxa		[%l7]0x88, %g2
	add		%fp, 2047-8, %l7
	ldxa		[%l7]0x88, %g3		! %g3:%g2 is tweak

	sethi		%hi(0x76543210), %l7
	or		%l7, %lo(0x76543210), %l7
	.word	0x81b5c320 !bmask	%l7,%g0,%g0		! byte swap mask

	prefetch	[%i0], 20
	prefetch	[%i0 + 63], 20
	call		_aes256_load_deckey
	and		%i2, 15,  %i5
	and		%i2, -16, %i2
	mov		0, %l7
	movrnz		%i5, 16,  %l7
	sub		%i2, %l7, %i2

	sub		%i0, %i1, %l5	! %i0!=%i1
	and		%i0, 7, %l0
	andn		%i0, 7, %i0
	sll		%l0, 3, %l0
	mov		64, %l1
	mov		0xff, %l3
	sub		%l1, %l0, %l1
	and		%i1, 7, %l2
	cmp		%i2, 255
	movrnz		%l2, 0, %l5		! if (	%i1&7 ||
	movleu		%xcc, 0, %l5	!	%i2<256 ||
	brnz,pn		%l5, .L256_xts_deblk !	%i0==%i1)
	srl		%l3, %l2, %l3

	andcc		%i2, 16, %g0		! is number of blocks even?
	brz,pn		%i2, .L256_xts_desteal
	.word	0xb3b64340 !alignaddrl	%i1,%g0,%i1
	bz		%icc, .L256_xts_deloop2x
	srlx		%i2, 4, %i2
.L256_xts_deloop:
	ldx		[%i0 + 0], %o0
	brz,pt		%l0, 4f
	ldx		[%i0 + 8], %o1

	ldx		[%i0 + 16], %o2
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	sllx		%o1, %l0, %o1
	or		%g1, %o0, %o0
	srlx		%o2, %l1, %o2
	or		%o2, %o1, %o1
4:
	.word	0x99b02302 !movxtod	%g2,%f12
	.word	0x9db02303 !movxtod	%g3,%f14
	.word	0x99b3098c !bshuffle	%f12,%f12,%f12
	.word	0x9db3898e !bshuffle	%f14,%f14,%f14

	xor		%g4, %o0, %o0		! ^= rk[0]
	xor		%g5, %o1, %o1
	.word	0x81b02308 !movxtod	%o0,%f0
	.word	0x85b02309 !movxtod	%o1,%f2

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= tweak[0]
	.word	0x85b38d82 !fxor	%f14,%f2,%f2

	prefetch	[%i1 + 63], 22
	prefetch	[%i0 + 16+63], 20
	call		_aes256_decrypt_1x
	add		%i0, 16, %i0

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= tweak[0]
	.word	0x85b38d82 !fxor	%f14,%f2,%f2

	srax		%g3, 63, %l7		! next tweak value
	addcc		%g2, %g2, %g2
	and		%l7, 0x87, %l7
	.word	0x87b0c223 !addxc	%g3,%g3,%g3
	xor		%l7, %g2, %g2

	brnz,pn		%l2, 2f
	sub		%i2, 1, %i2
		
	std		%f0, [%i1 + 0]
	std		%f2, [%i1 + 8]
	brnz,pt		%i2, .L256_xts_deloop2x
	add		%i1, 16, %i1

	brnz,pn		%i5, .L256_xts_desteal
	nop

	ret
	restore

.align	16
2:	ldxa		[%i0]0x82, %o0		! avoid read-after-write hazard
						! and ~3x deterioration
						! in inp==out case
	.word	0x89b00900 !faligndata	%f0,%f0,%f4		! handle unaligned output
	.word	0x8db00902 !faligndata	%f0,%f2,%f6
	.word	0x91b08902 !faligndata	%f2,%f2,%f8
	stda		%f4, [%i1 + %l3]0xc0	! partial store
	std		%f6, [%i1 + 8]
	add		%i1, 16, %i1
	orn		%g0, %l3, %l3
	stda		%f8, [%i1 + %l3]0xc0	! partial store

	brnz,pt		%i2, .L256_xts_deloop2x+4
	orn		%g0, %l3, %l3

	brnz,pn		%i5, .L256_xts_desteal
	nop

	ret
	restore

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
.align	32
.L256_xts_deloop2x:
	ldx		[%i0 + 0], %o0
	ldx		[%i0 + 8], %o1
	ldx		[%i0 + 16], %o2
	brz,pt		%l0, 4f
	ldx		[%i0 + 24], %o3

	ldx		[%i0 + 32], %o4
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	or		%g1, %o0, %o0
	sllx		%o1, %l0, %o1
	srlx		%o2, %l1, %g1
	or		%g1, %o1, %o1
	sllx		%o2, %l0, %o2
	srlx		%o3, %l1, %g1
	or		%g1, %o2, %o2
	sllx		%o3, %l0, %o3
	srlx		%o4, %l1, %o4
	or		%o4, %o3, %o3
4:
	.word	0x99b02302 !movxtod	%g2,%f12
	.word	0x9db02303 !movxtod	%g3,%f14
	.word	0x99b3098c !bshuffle	%f12,%f12,%f12
	.word	0x9db3898e !bshuffle	%f14,%f14,%f14

	srax		%g3, 63, %l7		! next tweak value
	addcc		%g2, %g2, %g2
	and		%l7, 0x87, %l7
	.word	0x87b0c223 !addxc	%g3,%g3,%g3
	xor		%l7, %g2, %g2

	.word	0x91b02302 !movxtod	%g2,%f8
	.word	0x95b02303 !movxtod	%g3,%f10
	.word	0x91b20988 !bshuffle	%f8,%f8,%f8
	.word	0x95b2898a !bshuffle	%f10,%f10,%f10

	xor		%g4, %o0, %o0		! ^= rk[0]
	xor		%g5, %o1, %o1
	xor		%g4, %o2, %o2		! ^= rk[0]
	xor		%g5, %o3, %o3
	.word	0x81b02308 !movxtod	%o0,%f0
	.word	0x85b02309 !movxtod	%o1,%f2
	.word	0x89b0230a !movxtod	%o2,%f4
	.word	0x8db0230b !movxtod	%o3,%f6

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= tweak[0]
	.word	0x85b38d82 !fxor	%f14,%f2,%f2
	.word	0x89b20d84 !fxor	%f8,%f4,%f4		! ^= tweak[0]
	.word	0x8db28d86 !fxor	%f10,%f6,%f6

	prefetch	[%i1 + 63], 22
	prefetch	[%i0 + 32+63], 20
	call		_aes256_decrypt_2x
	add		%i0, 32, %i0

	.word	0x91b02302 !movxtod	%g2,%f8
	.word	0x95b02303 !movxtod	%g3,%f10

	srax		%g3, 63, %l7		! next tweak value
	addcc		%g2, %g2, %g2
	and		%l7, 0x87, %l7
	.word	0x87b0c223 !addxc	%g3,%g3,%g3
	xor		%l7, %g2, %g2

	.word	0x91b20988 !bshuffle	%f8,%f8,%f8
	.word	0x95b2898a !bshuffle	%f10,%f10,%f10

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= tweak[0]
	.word	0x85b38d82 !fxor	%f14,%f2,%f2
	.word	0x89b20d84 !fxor	%f8,%f4,%f4
	.word	0x8db28d86 !fxor	%f10,%f6,%f6

	brnz,pn		%l2, 2f
	sub		%i2, 2, %i2
		
	std		%f0, [%i1 + 0]
	std		%f2, [%i1 + 8]
	std		%f4, [%i1 + 16]
	std		%f6, [%i1 + 24]
	brnz,pt		%i2, .L256_xts_deloop2x
	add		%i1, 32, %i1

	.word	0x81b00f04 !fsrc2	%f0,%f4,%f0
	.word	0x85b00f06 !fsrc2	%f0,%f6,%f2
	brnz,pn		%i5, .L256_xts_desteal
	nop

	ret
	restore

.align	16
2:	ldxa		[%i0]0x82, %o0		! avoid read-after-write hazard
						! and ~3x deterioration
						! in inp==out case
	.word	0x91b00900 !faligndata	%f0,%f0,%f8		! handle unaligned output
	.word	0x95b00902 !faligndata	%f0,%f2,%f10
	.word	0x99b08904 !faligndata	%f2,%f4,%f12
	.word	0x9db10906 !faligndata	%f4,%f6,%f14
	.word	0x81b18906 !faligndata	%f6,%f6,%f0

	stda		%f8, [%i1 + %l3]0xc0	! partial store
	std		%f10, [%i1 + 8]
	std		%f12, [%i1 + 16]
	std		%f14, [%i1 + 24]
	add		%i1, 32, %i1
	orn		%g0, %l3, %l3
	stda		%f0, [%i1 + %l3]0xc0	! partial store

	brnz,pt		%i2, .L256_xts_deloop2x+4
	orn		%g0, %l3, %l3

	.word	0x81b00f04 !fsrc2	%f0,%f4,%f0
	.word	0x85b00f06 !fsrc2	%f0,%f6,%f2
	brnz,pn		%i5, .L256_xts_desteal
	nop

	ret
	restore

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
.align	32
.L256_xts_deblk:
	add	%i1, %i2, %l5
	and	%l5, 63, %l5	! tail
	sub	%i2, %l5, %i2
	add	%l5, 15, %l5	! round up to 16n
	srlx	%i2, 4, %i2
	srl	%l5, 4, %l5
	sub	%i2, 1, %i2
	add	%l5, 1, %l5

.L256_xts_deblk2x:
	ldx		[%i0 + 0], %o0
	ldx		[%i0 + 8], %o1
	ldx		[%i0 + 16], %o2
	brz,pt		%l0, 5f
	ldx		[%i0 + 24], %o3

	ldx		[%i0 + 32], %o4
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	or		%g1, %o0, %o0
	sllx		%o1, %l0, %o1
	srlx		%o2, %l1, %g1
	or		%g1, %o1, %o1
	sllx		%o2, %l0, %o2
	srlx		%o3, %l1, %g1
	or		%g1, %o2, %o2
	sllx		%o3, %l0, %o3
	srlx		%o4, %l1, %o4
	or		%o4, %o3, %o3
5:
	.word	0x99b02302 !movxtod	%g2,%f12
	.word	0x9db02303 !movxtod	%g3,%f14
	.word	0x99b3098c !bshuffle	%f12,%f12,%f12
	.word	0x9db3898e !bshuffle	%f14,%f14,%f14

	srax		%g3, 63, %l7		! next tweak value
	addcc		%g2, %g2, %g2
	and		%l7, 0x87, %l7
	.word	0x87b0c223 !addxc	%g3,%g3,%g3
	xor		%l7, %g2, %g2

	.word	0x91b02302 !movxtod	%g2,%f8
	.word	0x95b02303 !movxtod	%g3,%f10
	.word	0x91b20988 !bshuffle	%f8,%f8,%f8
	.word	0x95b2898a !bshuffle	%f10,%f10,%f10

	xor		%g4, %o0, %o0		! ^= rk[0]
	xor		%g5, %o1, %o1
	xor		%g4, %o2, %o2		! ^= rk[0]
	xor		%g5, %o3, %o3
	.word	0x81b02308 !movxtod	%o0,%f0
	.word	0x85b02309 !movxtod	%o1,%f2
	.word	0x89b0230a !movxtod	%o2,%f4
	.word	0x8db0230b !movxtod	%o3,%f6

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= tweak[0]
	.word	0x85b38d82 !fxor	%f14,%f2,%f2
	.word	0x89b20d84 !fxor	%f8,%f4,%f4		! ^= tweak[0]
	.word	0x8db28d86 !fxor	%f10,%f6,%f6

	prefetch	[%i0 + 32+63], 20
	call		_aes256_decrypt_2x
	add		%i0, 32, %i0

	.word	0x91b02302 !movxtod	%g2,%f8
	.word	0x95b02303 !movxtod	%g3,%f10

	srax		%g3, 63, %l7		! next tweak value
	addcc		%g2, %g2, %g2
	and		%l7, 0x87, %l7
	.word	0x87b0c223 !addxc	%g3,%g3,%g3
	xor		%l7, %g2, %g2

	.word	0x91b20988 !bshuffle	%f8,%f8,%f8
	.word	0x95b2898a !bshuffle	%f10,%f10,%f10

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= tweak[0]
	.word	0x85b38d82 !fxor	%f14,%f2,%f2
	.word	0x89b20d84 !fxor	%f8,%f4,%f4
	.word	0x8db28d86 !fxor	%f10,%f6,%f6

	subcc		%i2, 2, %i2
	stda		%f0, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	add		%i1, 8, %i1
	stda		%f2, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	add		%i1, 8, %i1
	stda		%f4, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	add		%i1, 8, %i1
	stda		%f6, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	bgu,pt		%xcc, .L256_xts_deblk2x
	add		%i1, 8, %i1

	add		%l5, %i2, %i2
	andcc		%i2, 1, %g0		! is number of blocks even?
	membar		#StoreLoad|#StoreStore
	bnz,pt		%icc, .L256_xts_deloop
	srl		%i2, 0, %i2
	brnz,pn		%i2, .L256_xts_deloop2x
	nop

	.word	0x81b00f04 !fsrc2	%f0,%f4,%f0
	.word	0x85b00f06 !fsrc2	%f0,%f6,%f2
	brnz,pn		%i5, .L256_xts_desteal
	nop

	ret
	restore
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
.align	32
.L256_xts_desteal:
	ldx		[%i0 + 0], %o0
	brz,pt		%l0, 8f
	ldx		[%i0 + 8], %o1

	ldx		[%i0 + 16], %o2
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	sllx		%o1, %l0, %o1
	or		%g1, %o0, %o0
	srlx		%o2, %l1, %o2
	or		%o2, %o1, %o1
8:
	srax		%g3, 63, %l7		! next tweak value
	addcc		%g2, %g2, %o2
	and		%l7, 0x87, %l7
	.word	0x97b0c223 !addxc	%g3,%g3,%o3
	xor		%l7, %o2, %o2

	.word	0x99b0230a !movxtod	%o2,%f12
	.word	0x9db0230b !movxtod	%o3,%f14
	.word	0x99b3098c !bshuffle	%f12,%f12,%f12
	.word	0x9db3898e !bshuffle	%f14,%f14,%f14

	xor		%g4, %o0, %o0		! ^= rk[0]
	xor		%g5, %o1, %o1
	.word	0x81b02308 !movxtod	%o0,%f0
	.word	0x85b02309 !movxtod	%o1,%f2

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= tweak[0]
	.word	0x85b38d82 !fxor	%f14,%f2,%f2

	call		_aes256_decrypt_1x
	add		%i0, 16, %i0

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= tweak[0]
	.word	0x85b38d82 !fxor	%f14,%f2,%f2

	std		%f0, [%fp + 2047-16]
	std		%f2, [%fp + 2047-8]

	srl		%l0, 3, %l0
	add		%fp, 2047-16, %l7
	add		%i0, %l0, %i0	! original %i0+%i2&-15
	add		%i1, %l2, %i1	! original %i1+%i2&-15
	mov		0, %l0
	add		%i1, 16, %i1
	nop					! align

.L256_xts_destealing:
	ldub		[%i0 + %l0], %o0
	ldub		[%l7  + %l0], %o1
	dec		%i5
	stb		%o0, [%l7  + %l0]
	stb		%o1, [%i1 + %l0]
	brnz		%i5, .L256_xts_destealing
	inc		%l0

	mov		%l7, %i0
	sub		%i1, 16, %i1
	mov		0, %l0
	sub		%i1, %l2, %i1
	ba		.L256_xts_deloop	! one more time
	mov		1, %i2				! %i5 is 0
	ret
	restore
.type	aes256_t4_xts_decrypt,#function
.size	aes256_t4_xts_decrypt,.-aes256_t4_xts_decrypt
.globl	aes192_t4_ctr32_encrypt
.align	32
aes192_t4_ctr32_encrypt:
	save		%sp, -192, %sp
	srln		%i2, 0, %i2		! needed on v8+, "nop" on v9

	prefetch	[%i0], 20
	prefetch	[%i0 + 63], 20
	call		_aes192_load_enckey
	sllx		%i2, 4, %i2

	ld		[%i4 + 0], %l4	! counter
	ld		[%i4 + 4], %l5
	ld		[%i4 + 8], %l6
	ld		[%i4 + 12], %l7

	sllx		%l4, 32, %o5
	or		%l5, %o5, %o5
	sllx		%l6, 32, %g1
	xor		%o5, %g4, %g4		! ^= rk[0]
	xor		%g1, %g5, %g5
	.word	0x9db02304 !movxtod	%g4,%f14		! most significant 64 bits

	sub		%i0, %i1, %l5	! %i0!=%i1
	and		%i0, 7, %l0
	andn		%i0, 7, %i0
	sll		%l0, 3, %l0
	mov		64, %l1
	mov		0xff, %l3
	sub		%l1, %l0, %l1
	and		%i1, 7, %l2
	cmp		%i2, 255
	movrnz		%l2, 0, %l5		! if (	%i1&7 ||
	movleu		%xcc, 0, %l5	!	%i2<256 ||
	brnz,pn		%l5, .L192_ctr32_blk	!	%i0==%i1)
	srl		%l3, %l2, %l3

	andcc		%i2, 16, %g0		! is number of blocks even?
	.word	0xb3b64340 !alignaddrl	%i1,%g0,%i1
	bz		%icc, .L192_ctr32_loop2x
	srlx		%i2, 4, %i2
.L192_ctr32_loop:
	ldx		[%i0 + 0], %o0
	brz,pt		%l0, 4f
	ldx		[%i0 + 8], %o1

	ldx		[%i0 + 16], %o2
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	sllx		%o1, %l0, %o1
	or		%g1, %o0, %o0
	srlx		%o2, %l1, %o2
	or		%o2, %o1, %o1
4:
	xor		%g5, %l7, %g1		! ^= rk[0]
	add		%l7, 1, %l7
	.word	0x85b02301 !movxtod	%g1,%f2
	srl		%l7, 0, %l7		! clruw
	prefetch	[%i1 + 63], 22
	prefetch	[%i0 + 16+63], 20
	.word	0x88cc040e !aes_eround01	%f16,%f14,%f2,%f4
	.word	0x84cc842e !aes_eround23	%f18,%f14,%f2,%f2
	call		_aes192_encrypt_1x+8
	add		%i0, 16, %i0

	.word	0x95b02308 !movxtod	%o0,%f10
	.word	0x99b02309 !movxtod	%o1,%f12
	.word	0x81b28d80 !fxor	%f10,%f0,%f0		! ^= inp
	.word	0x85b30d82 !fxor	%f12,%f2,%f2

	brnz,pn		%l2, 2f
	sub		%i2, 1, %i2
		
	std		%f0, [%i1 + 0]
	std		%f2, [%i1 + 8]
	brnz,pt		%i2, .L192_ctr32_loop2x
	add		%i1, 16, %i1

	ret
	restore

.align	16
2:	ldxa		[%i0]0x82, %o0		! avoid read-after-write hazard
						! and ~3x deterioration
						! in inp==out case
	.word	0x89b00900 !faligndata	%f0,%f0,%f4		! handle unaligned output
	.word	0x8db00902 !faligndata	%f0,%f2,%f6
	.word	0x91b08902 !faligndata	%f2,%f2,%f8
	stda		%f4, [%i1 + %l3]0xc0	! partial store
	std		%f6, [%i1 + 8]
	add		%i1, 16, %i1
	orn		%g0, %l3, %l3
	stda		%f8, [%i1 + %l3]0xc0	! partial store

	brnz,pt		%i2, .L192_ctr32_loop2x+4
	orn		%g0, %l3, %l3

	ret
	restore

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
.align	32
.L192_ctr32_loop2x:
	ldx		[%i0 + 0], %o0
	ldx		[%i0 + 8], %o1
	ldx		[%i0 + 16], %o2
	brz,pt		%l0, 4f
	ldx		[%i0 + 24], %o3

	ldx		[%i0 + 32], %o4
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	or		%g1, %o0, %o0
	sllx		%o1, %l0, %o1
	srlx		%o2, %l1, %g1
	or		%g1, %o1, %o1
	sllx		%o2, %l0, %o2
	srlx		%o3, %l1, %g1
	or		%g1, %o2, %o2
	sllx		%o3, %l0, %o3
	srlx		%o4, %l1, %o4
	or		%o4, %o3, %o3
4:
	xor		%g5, %l7, %g1		! ^= rk[0]
	add		%l7, 1, %l7
	.word	0x85b02301 !movxtod	%g1,%f2
	srl		%l7, 0, %l7		! clruw
	xor		%g5, %l7, %g1
	add		%l7, 1, %l7
	.word	0x8db02301 !movxtod	%g1,%f6
	srl		%l7, 0, %l7		! clruw
	prefetch	[%i1 + 63], 22
	prefetch	[%i0 + 32+63], 20
	.word	0x90cc040e !aes_eround01	%f16,%f14,%f2,%f8
	.word	0x84cc842e !aes_eround23	%f18,%f14,%f2,%f2
	.word	0x94cc0c0e !aes_eround01	%f16,%f14,%f6,%f10
	.word	0x8ccc8c2e !aes_eround23	%f18,%f14,%f6,%f6
	call		_aes192_encrypt_2x+16
	add		%i0, 32, %i0

	.word	0x91b02308 !movxtod	%o0,%f8
	.word	0x95b02309 !movxtod	%o1,%f10
	.word	0x99b0230a !movxtod	%o2,%f12
	.word	0x81b20d80 !fxor	%f8,%f0,%f0		! ^= inp
	.word	0x91b0230b !movxtod	%o3,%f8
	.word	0x85b28d82 !fxor	%f10,%f2,%f2
	.word	0x89b30d84 !fxor	%f12,%f4,%f4
	.word	0x8db20d86 !fxor	%f8,%f6,%f6

	brnz,pn		%l2, 2f
	sub		%i2, 2, %i2
		
	std		%f0, [%i1 + 0]
	std		%f2, [%i1 + 8]
	std		%f4, [%i1 + 16]
	std		%f6, [%i1 + 24]
	brnz,pt		%i2, .L192_ctr32_loop2x
	add		%i1, 32, %i1

	ret
	restore

.align	16
2:	ldxa		[%i0]0x82, %o0		! avoid read-after-write hazard
						! and ~3x deterioration
						! in inp==out case
	.word	0x91b00900 !faligndata	%f0,%f0,%f8		! handle unaligned output
	.word	0x81b00902 !faligndata	%f0,%f2,%f0
	.word	0x85b08904 !faligndata	%f2,%f4,%f2
	.word	0x89b10906 !faligndata	%f4,%f6,%f4
	.word	0x8db18906 !faligndata	%f6,%f6,%f6

	stda		%f8, [%i1 + %l3]0xc0	! partial store
	std		%f0, [%i1 + 8]
	std		%f2, [%i1 + 16]
	std		%f4, [%i1 + 24]
	add		%i1, 32, %i1
	orn		%g0, %l3, %l3
	stda		%f6, [%i1 + %l3]0xc0	! partial store

	brnz,pt		%i2, .L192_ctr32_loop2x+4
	orn		%g0, %l3, %l3

	ret
	restore

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
.align	32
.L192_ctr32_blk:
	add	%i1, %i2, %l5
	and	%l5, 63, %l5	! tail
	sub	%i2, %l5, %i2
	add	%l5, 15, %l5	! round up to 16n
	srlx	%i2, 4, %i2
	srl	%l5, 4, %l5
	sub	%i2, 1, %i2
	add	%l5, 1, %l5

.L192_ctr32_blk_loop2x:
	ldx		[%i0 + 0], %o0
	ldx		[%i0 + 8], %o1
	ldx		[%i0 + 16], %o2
	brz,pt		%l0, 5f
	ldx		[%i0 + 24], %o3

	ldx		[%i0 + 32], %o4
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	or		%g1, %o0, %o0
	sllx		%o1, %l0, %o1
	srlx		%o2, %l1, %g1
	or		%g1, %o1, %o1
	sllx		%o2, %l0, %o2
	srlx		%o3, %l1, %g1
	or		%g1, %o2, %o2
	sllx		%o3, %l0, %o3
	srlx		%o4, %l1, %o4
	or		%o4, %o3, %o3
5:
	xor		%g5, %l7, %g1		! ^= rk[0]
	add		%l7, 1, %l7
	.word	0x85b02301 !movxtod	%g1,%f2
	srl		%l7, 0, %l7		! clruw
	xor		%g5, %l7, %g1
	add		%l7, 1, %l7
	.word	0x8db02301 !movxtod	%g1,%f6
	srl		%l7, 0, %l7		! clruw
	prefetch	[%i0 + 32+63], 20
	.word	0x90cc040e !aes_eround01	%f16,%f14,%f2,%f8
	.word	0x84cc842e !aes_eround23	%f18,%f14,%f2,%f2
	.word	0x94cc0c0e !aes_eround01	%f16,%f14,%f6,%f10
	.word	0x8ccc8c2e !aes_eround23	%f18,%f14,%f6,%f6
	call		_aes192_encrypt_2x+16
	add		%i0, 32, %i0
	subcc		%i2, 2, %i2

	.word	0x91b02308 !movxtod	%o0,%f8
	.word	0x95b02309 !movxtod	%o1,%f10
	.word	0x99b0230a !movxtod	%o2,%f12
	.word	0x81b20d80 !fxor	%f8,%f0,%f0		! ^= inp
	.word	0x91b0230b !movxtod	%o3,%f8
	.word	0x85b28d82 !fxor	%f10,%f2,%f2
	.word	0x89b30d84 !fxor	%f12,%f4,%f4
	.word	0x8db20d86 !fxor	%f8,%f6,%f6

	stda		%f0, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	add		%i1, 8, %i1
	stda		%f2, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	add		%i1, 8, %i1
	stda		%f4, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	add		%i1, 8, %i1
	stda		%f6, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	bgu,pt		%xcc, .L192_ctr32_blk_loop2x
	add		%i1, 8, %i1

	add		%l5, %i2, %i2
	andcc		%i2, 1, %g0		! is number of blocks even?
	membar		#StoreLoad|#StoreStore
	bnz,pt		%icc, .L192_ctr32_loop
	srl		%i2, 0, %i2
	brnz,pn		%i2, .L192_ctr32_loop2x
	nop

	ret
	restore
.type	aes192_t4_ctr32_encrypt,#function
.size	aes192_t4_ctr32_encrypt,.-aes192_t4_ctr32_encrypt
.globl	aes192_t4_cbc_decrypt
.align	32
aes192_t4_cbc_decrypt:
	save		%sp, -192, %sp
	cmp		%i2, 0
	be,pn		%xcc, .L192_cbc_dec_abort
	srln		%i2, 0, %i2		! needed on v8+, "nop" on v9
	sub		%i0, %i1, %l5	! %i0!=%i1
	ld		[%i4 + 0], %f12	! load ivec
	ld		[%i4 + 4], %f13
	ld		[%i4 + 8], %f14
	ld		[%i4 + 12], %f15
	prefetch	[%i0], 20
	prefetch	[%i0 + 63], 20
	call		_aes192_load_deckey
	and		%i0, 7, %l0
	andn		%i0, 7, %i0
	sll		%l0, 3, %l0
	mov		64, %l1
	mov		0xff, %l3
	sub		%l1, %l0, %l1
	and		%i1, 7, %l2
	cmp		%i2, 255
	movrnz		%l2, 0, %l5		! if (	%i1&7 ||
	movleu		%xcc, 0, %l5	!	%i2<256 ||
	brnz,pn		%l5, .L192cbc_dec_blk	!	%i0==%i1)
	srl		%l3, %l2, %l3

	andcc		%i2, 16, %g0		! is number of blocks even?
	srlx		%i2, 4, %i2
	.word	0xb3b64340 !alignaddrl	%i1,%g0,%i1
	bz		%icc, .L192_cbc_dec_loop2x
	prefetch	[%i1], 22
.L192_cbc_dec_loop:
	ldx		[%i0 + 0], %o0
	brz,pt		%l0, 4f
	ldx		[%i0 + 8], %o1

	ldx		[%i0 + 16], %o2
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	sllx		%o1, %l0, %o1
	or		%g1, %o0, %o0
	srlx		%o2, %l1, %o2
	or		%o2, %o1, %o1
4:
	xor		%g4, %o0, %o2		! ^= rk[0]
	xor		%g5, %o1, %o3
	.word	0x81b0230a !movxtod	%o2,%f0
	.word	0x85b0230b !movxtod	%o3,%f2

	prefetch	[%i1 + 63], 22
	prefetch	[%i0 + 16+63], 20
	call		_aes192_decrypt_1x
	add		%i0, 16, %i0

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= ivec
	.word	0x85b38d82 !fxor	%f14,%f2,%f2
	.word	0x99b02308 !movxtod	%o0,%f12
	.word	0x9db02309 !movxtod	%o1,%f14

	brnz,pn		%l2, 2f
	sub		%i2, 1, %i2
		
	std		%f0, [%i1 + 0]
	std		%f2, [%i1 + 8]
	brnz,pt		%i2, .L192_cbc_dec_loop2x
	add		%i1, 16, %i1
	st		%f12, [%i4 + 0]
	st		%f13, [%i4 + 4]
	st		%f14, [%i4 + 8]
	st		%f15, [%i4 + 12]
.L192_cbc_dec_abort:
	ret
	restore

.align	16
2:	ldxa		[%i0]0x82, %o0		! avoid read-after-write hazard
						! and ~3x deterioration
						! in inp==out case
	.word	0x89b00900 !faligndata	%f0,%f0,%f4		! handle unaligned output
	.word	0x8db00902 !faligndata	%f0,%f2,%f6
	.word	0x91b08902 !faligndata	%f2,%f2,%f8

	stda		%f4, [%i1 + %l3]0xc0	! partial store
	std		%f6, [%i1 + 8]
	add		%i1, 16, %i1
	orn		%g0, %l3, %l3
	stda		%f8, [%i1 + %l3]0xc0	! partial store

	brnz,pt		%i2, .L192_cbc_dec_loop2x+4
	orn		%g0, %l3, %l3
	st		%f12, [%i4 + 0]
	st		%f13, [%i4 + 4]
	st		%f14, [%i4 + 8]
	st		%f15, [%i4 + 12]
	ret
	restore

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
.align	32
.L192_cbc_dec_loop2x:
	ldx		[%i0 + 0], %o0
	ldx		[%i0 + 8], %o1
	ldx		[%i0 + 16], %o2
	brz,pt		%l0, 4f
	ldx		[%i0 + 24], %o3

	ldx		[%i0 + 32], %o4
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	or		%g1, %o0, %o0
	sllx		%o1, %l0, %o1
	srlx		%o2, %l1, %g1
	or		%g1, %o1, %o1
	sllx		%o2, %l0, %o2
	srlx		%o3, %l1, %g1
	or		%g1, %o2, %o2
	sllx		%o3, %l0, %o3
	srlx		%o4, %l1, %o4
	or		%o4, %o3, %o3
4:
	xor		%g4, %o0, %o4		! ^= rk[0]
	xor		%g5, %o1, %o5
	.word	0x81b0230c !movxtod	%o4,%f0
	.word	0x85b0230d !movxtod	%o5,%f2
	xor		%g4, %o2, %o4
	xor		%g5, %o3, %o5
	.word	0x89b0230c !movxtod	%o4,%f4
	.word	0x8db0230d !movxtod	%o5,%f6

	prefetch	[%i1 + 63], 22
	prefetch	[%i0 + 32+63], 20
	call		_aes192_decrypt_2x
	add		%i0, 32, %i0

	.word	0x91b02308 !movxtod	%o0,%f8
	.word	0x95b02309 !movxtod	%o1,%f10
	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= ivec
	.word	0x85b38d82 !fxor	%f14,%f2,%f2
	.word	0x99b0230a !movxtod	%o2,%f12
	.word	0x9db0230b !movxtod	%o3,%f14
	.word	0x89b20d84 !fxor	%f8,%f4,%f4
	.word	0x8db28d86 !fxor	%f10,%f6,%f6

	brnz,pn		%l2, 2f
	sub		%i2, 2, %i2
		
	std		%f0, [%i1 + 0]
	std		%f2, [%i1 + 8]
	std		%f4, [%i1 + 16]
	std		%f6, [%i1 + 24]
	brnz,pt		%i2, .L192_cbc_dec_loop2x
	add		%i1, 32, %i1
	st		%f12, [%i4 + 0]
	st		%f13, [%i4 + 4]
	st		%f14, [%i4 + 8]
	st		%f15, [%i4 + 12]
	ret
	restore

.align	16
2:	ldxa		[%i0]0x82, %o0		! avoid read-after-write hazard
						! and ~3x deterioration
						! in inp==out case
	.word	0x91b00900 !faligndata	%f0,%f0,%f8		! handle unaligned output
	.word	0x81b00902 !faligndata	%f0,%f2,%f0
	.word	0x85b08904 !faligndata	%f2,%f4,%f2
	.word	0x89b10906 !faligndata	%f4,%f6,%f4
	.word	0x8db18906 !faligndata	%f6,%f6,%f6
	stda		%f8, [%i1 + %l3]0xc0	! partial store
	std		%f0, [%i1 + 8]
	std		%f2, [%i1 + 16]
	std		%f4, [%i1 + 24]
	add		%i1, 32, %i1
	orn		%g0, %l3, %l3
	stda		%f6, [%i1 + %l3]0xc0	! partial store

	brnz,pt		%i2, .L192_cbc_dec_loop2x+4
	orn		%g0, %l3, %l3
	st		%f12, [%i4 + 0]
	st		%f13, [%i4 + 4]
	st		%f14, [%i4 + 8]
	st		%f15, [%i4 + 12]
	ret
	restore

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
.align	32
.L192cbc_dec_blk:
	add	%i1, %i2, %l5
	and	%l5, 63, %l5	! tail
	sub	%i2, %l5, %i2
	add	%l5, 15, %l5	! round up to 16n
	srlx	%i2, 4, %i2
	srl	%l5, 4, %l5
	sub	%i2, 1, %i2
	add	%l5, 1, %l5

.L192_cbc_dec_blk_loop2x:
	ldx		[%i0 + 0], %o0
	ldx		[%i0 + 8], %o1
	ldx		[%i0 + 16], %o2
	brz,pt		%l0, 5f
	ldx		[%i0 + 24], %o3

	ldx		[%i0 + 32], %o4
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	or		%g1, %o0, %o0
	sllx		%o1, %l0, %o1
	srlx		%o2, %l1, %g1
	or		%g1, %o1, %o1
	sllx		%o2, %l0, %o2
	srlx		%o3, %l1, %g1
	or		%g1, %o2, %o2
	sllx		%o3, %l0, %o3
	srlx		%o4, %l1, %o4
	or		%o4, %o3, %o3
5:
	xor		%g4, %o0, %o4		! ^= rk[0]
	xor		%g5, %o1, %o5
	.word	0x81b0230c !movxtod	%o4,%f0
	.word	0x85b0230d !movxtod	%o5,%f2
	xor		%g4, %o2, %o4
	xor		%g5, %o3, %o5
	.word	0x89b0230c !movxtod	%o4,%f4
	.word	0x8db0230d !movxtod	%o5,%f6

	prefetch	[%i0 + 32+63], 20
	call		_aes192_decrypt_2x
	add		%i0, 32, %i0
	subcc		%i2, 2, %i2

	.word	0x91b02308 !movxtod	%o0,%f8
	.word	0x95b02309 !movxtod	%o1,%f10
	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= ivec
	.word	0x85b38d82 !fxor	%f14,%f2,%f2
	.word	0x99b0230a !movxtod	%o2,%f12
	.word	0x9db0230b !movxtod	%o3,%f14
	.word	0x89b20d84 !fxor	%f8,%f4,%f4
	.word	0x8db28d86 !fxor	%f10,%f6,%f6

	stda		%f0, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	add		%i1, 8, %i1
	stda		%f2, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	add		%i1, 8, %i1
	stda		%f4, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	add		%i1, 8, %i1
	stda		%f6, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	bgu,pt		%xcc, .L192_cbc_dec_blk_loop2x
	add		%i1, 8, %i1

	add		%l5, %i2, %i2
	andcc		%i2, 1, %g0		! is number of blocks even?
	membar		#StoreLoad|#StoreStore
	bnz,pt		%icc, .L192_cbc_dec_loop
	srl		%i2, 0, %i2
	brnz,pn		%i2, .L192_cbc_dec_loop2x
	nop
	st		%f12, [%i4 + 0]	! write out ivec
	st		%f13, [%i4 + 4]
	st		%f14, [%i4 + 8]
	st		%f15, [%i4 + 12]
	ret
	restore
.type	aes192_t4_cbc_decrypt,#function
.size	aes192_t4_cbc_decrypt,.-aes192_t4_cbc_decrypt
.globl	aes256_t4_cbc_decrypt
.align	32
aes256_t4_cbc_decrypt:
	save		%sp, -192, %sp
	cmp		%i2, 0
	be,pn		%xcc, .L256_cbc_dec_abort
	srln		%i2, 0, %i2		! needed on v8+, "nop" on v9
	sub		%i0, %i1, %l5	! %i0!=%i1
	ld		[%i4 + 0], %f12	! load ivec
	ld		[%i4 + 4], %f13
	ld		[%i4 + 8], %f14
	ld		[%i4 + 12], %f15
	prefetch	[%i0], 20
	prefetch	[%i0 + 63], 20
	call		_aes256_load_deckey
	and		%i0, 7, %l0
	andn		%i0, 7, %i0
	sll		%l0, 3, %l0
	mov		64, %l1
	mov		0xff, %l3
	sub		%l1, %l0, %l1
	and		%i1, 7, %l2
	cmp		%i2, 255
	movrnz		%l2, 0, %l5		! if (	%i1&7 ||
	movleu		%xcc, 0, %l5	!	%i2<256 ||
	brnz,pn		%l5, .L256cbc_dec_blk	!	%i0==%i1)
	srl		%l3, %l2, %l3

	andcc		%i2, 16, %g0		! is number of blocks even?
	srlx		%i2, 4, %i2
	.word	0xb3b64340 !alignaddrl	%i1,%g0,%i1
	bz		%icc, .L256_cbc_dec_loop2x
	prefetch	[%i1], 22
.L256_cbc_dec_loop:
	ldx		[%i0 + 0], %o0
	brz,pt		%l0, 4f
	ldx		[%i0 + 8], %o1

	ldx		[%i0 + 16], %o2
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	sllx		%o1, %l0, %o1
	or		%g1, %o0, %o0
	srlx		%o2, %l1, %o2
	or		%o2, %o1, %o1
4:
	xor		%g4, %o0, %o2		! ^= rk[0]
	xor		%g5, %o1, %o3
	.word	0x81b0230a !movxtod	%o2,%f0
	.word	0x85b0230b !movxtod	%o3,%f2

	prefetch	[%i1 + 63], 22
	prefetch	[%i0 + 16+63], 20
	call		_aes256_decrypt_1x
	add		%i0, 16, %i0

	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= ivec
	.word	0x85b38d82 !fxor	%f14,%f2,%f2
	.word	0x99b02308 !movxtod	%o0,%f12
	.word	0x9db02309 !movxtod	%o1,%f14

	brnz,pn		%l2, 2f
	sub		%i2, 1, %i2
		
	std		%f0, [%i1 + 0]
	std		%f2, [%i1 + 8]
	brnz,pt		%i2, .L256_cbc_dec_loop2x
	add		%i1, 16, %i1
	st		%f12, [%i4 + 0]
	st		%f13, [%i4 + 4]
	st		%f14, [%i4 + 8]
	st		%f15, [%i4 + 12]
.L256_cbc_dec_abort:
	ret
	restore

.align	16
2:	ldxa		[%i0]0x82, %o0		! avoid read-after-write hazard
						! and ~3x deterioration
						! in inp==out case
	.word	0x89b00900 !faligndata	%f0,%f0,%f4		! handle unaligned output
	.word	0x8db00902 !faligndata	%f0,%f2,%f6
	.word	0x91b08902 !faligndata	%f2,%f2,%f8

	stda		%f4, [%i1 + %l3]0xc0	! partial store
	std		%f6, [%i1 + 8]
	add		%i1, 16, %i1
	orn		%g0, %l3, %l3
	stda		%f8, [%i1 + %l3]0xc0	! partial store

	brnz,pt		%i2, .L256_cbc_dec_loop2x+4
	orn		%g0, %l3, %l3
	st		%f12, [%i4 + 0]
	st		%f13, [%i4 + 4]
	st		%f14, [%i4 + 8]
	st		%f15, [%i4 + 12]
	ret
	restore

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
.align	32
.L256_cbc_dec_loop2x:
	ldx		[%i0 + 0], %o0
	ldx		[%i0 + 8], %o1
	ldx		[%i0 + 16], %o2
	brz,pt		%l0, 4f
	ldx		[%i0 + 24], %o3

	ldx		[%i0 + 32], %o4
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	or		%g1, %o0, %o0
	sllx		%o1, %l0, %o1
	srlx		%o2, %l1, %g1
	or		%g1, %o1, %o1
	sllx		%o2, %l0, %o2
	srlx		%o3, %l1, %g1
	or		%g1, %o2, %o2
	sllx		%o3, %l0, %o3
	srlx		%o4, %l1, %o4
	or		%o4, %o3, %o3
4:
	xor		%g4, %o0, %o4		! ^= rk[0]
	xor		%g5, %o1, %o5
	.word	0x81b0230c !movxtod	%o4,%f0
	.word	0x85b0230d !movxtod	%o5,%f2
	xor		%g4, %o2, %o4
	xor		%g5, %o3, %o5
	.word	0x89b0230c !movxtod	%o4,%f4
	.word	0x8db0230d !movxtod	%o5,%f6

	prefetch	[%i1 + 63], 22
	prefetch	[%i0 + 32+63], 20
	call		_aes256_decrypt_2x
	add		%i0, 32, %i0

	.word	0x91b02308 !movxtod	%o0,%f8
	.word	0x95b02309 !movxtod	%o1,%f10
	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= ivec
	.word	0x85b38d82 !fxor	%f14,%f2,%f2
	.word	0x99b0230a !movxtod	%o2,%f12
	.word	0x9db0230b !movxtod	%o3,%f14
	.word	0x89b20d84 !fxor	%f8,%f4,%f4
	.word	0x8db28d86 !fxor	%f10,%f6,%f6

	brnz,pn		%l2, 2f
	sub		%i2, 2, %i2
		
	std		%f0, [%i1 + 0]
	std		%f2, [%i1 + 8]
	std		%f4, [%i1 + 16]
	std		%f6, [%i1 + 24]
	brnz,pt		%i2, .L256_cbc_dec_loop2x
	add		%i1, 32, %i1
	st		%f12, [%i4 + 0]
	st		%f13, [%i4 + 4]
	st		%f14, [%i4 + 8]
	st		%f15, [%i4 + 12]
	ret
	restore

.align	16
2:	ldxa		[%i0]0x82, %o0		! avoid read-after-write hazard
						! and ~3x deterioration
						! in inp==out case
	.word	0x91b00900 !faligndata	%f0,%f0,%f8		! handle unaligned output
	.word	0x81b00902 !faligndata	%f0,%f2,%f0
	.word	0x85b08904 !faligndata	%f2,%f4,%f2
	.word	0x89b10906 !faligndata	%f4,%f6,%f4
	.word	0x8db18906 !faligndata	%f6,%f6,%f6
	stda		%f8, [%i1 + %l3]0xc0	! partial store
	std		%f0, [%i1 + 8]
	std		%f2, [%i1 + 16]
	std		%f4, [%i1 + 24]
	add		%i1, 32, %i1
	orn		%g0, %l3, %l3
	stda		%f6, [%i1 + %l3]0xc0	! partial store

	brnz,pt		%i2, .L256_cbc_dec_loop2x+4
	orn		%g0, %l3, %l3
	st		%f12, [%i4 + 0]
	st		%f13, [%i4 + 4]
	st		%f14, [%i4 + 8]
	st		%f15, [%i4 + 12]
	ret
	restore

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
.align	32
.L256cbc_dec_blk:
	add	%i1, %i2, %l5
	and	%l5, 63, %l5	! tail
	sub	%i2, %l5, %i2
	add	%l5, 15, %l5	! round up to 16n
	srlx	%i2, 4, %i2
	srl	%l5, 4, %l5
	sub	%i2, 1, %i2
	add	%l5, 1, %l5

.L256_cbc_dec_blk_loop2x:
	ldx		[%i0 + 0], %o0
	ldx		[%i0 + 8], %o1
	ldx		[%i0 + 16], %o2
	brz,pt		%l0, 5f
	ldx		[%i0 + 24], %o3

	ldx		[%i0 + 32], %o4
	sllx		%o0, %l0, %o0
	srlx		%o1, %l1, %g1
	or		%g1, %o0, %o0
	sllx		%o1, %l0, %o1
	srlx		%o2, %l1, %g1
	or		%g1, %o1, %o1
	sllx		%o2, %l0, %o2
	srlx		%o3, %l1, %g1
	or		%g1, %o2, %o2
	sllx		%o3, %l0, %o3
	srlx		%o4, %l1, %o4
	or		%o4, %o3, %o3
5:
	xor		%g4, %o0, %o4		! ^= rk[0]
	xor		%g5, %o1, %o5
	.word	0x81b0230c !movxtod	%o4,%f0
	.word	0x85b0230d !movxtod	%o5,%f2
	xor		%g4, %o2, %o4
	xor		%g5, %o3, %o5
	.word	0x89b0230c !movxtod	%o4,%f4
	.word	0x8db0230d !movxtod	%o5,%f6

	prefetch	[%i0 + 32+63], 20
	call		_aes256_decrypt_2x
	add		%i0, 32, %i0
	subcc		%i2, 2, %i2

	.word	0x91b02308 !movxtod	%o0,%f8
	.word	0x95b02309 !movxtod	%o1,%f10
	.word	0x81b30d80 !fxor	%f12,%f0,%f0		! ^= ivec
	.word	0x85b38d82 !fxor	%f14,%f2,%f2
	.word	0x99b0230a !movxtod	%o2,%f12
	.word	0x9db0230b !movxtod	%o3,%f14
	.word	0x89b20d84 !fxor	%f8,%f4,%f4
	.word	0x8db28d86 !fxor	%f10,%f6,%f6

	stda		%f0, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	add		%i1, 8, %i1
	stda		%f2, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	add		%i1, 8, %i1
	stda		%f4, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	add		%i1, 8, %i1
	stda		%f6, [%i1]0xe2		! ASI_BLK_INIT, T4-specific
	bgu,pt		%xcc, .L256_cbc_dec_blk_loop2x
	add		%i1, 8, %i1

	add		%l5, %i2, %i2
	andcc		%i2, 1, %g0		! is number of blocks even?
	membar		#StoreLoad|#StoreStore
	bnz,pt		%icc, .L256_cbc_dec_loop
	srl		%i2, 0, %i2
	brnz,pn		%i2, .L256_cbc_dec_loop2x
	nop
	st		%f12, [%i4 + 0]	! write out ivec
	st		%f13, [%i4 + 4]
	st		%f14, [%i4 + 8]
	st		%f15, [%i4 + 12]
	ret
	restore
.type	aes256_t4_cbc_decrypt,#function
.size	aes256_t4_cbc_decrypt,.-aes256_t4_cbc_decrypt
.align	32
_aes256_decrypt_1x:
	.word	0x88cc0440 !aes_dround01	%f16,%f0,%f2,%f4
	.word	0x84cc8460 !aes_dround23	%f18,%f0,%f2,%f2
	ldd		[%i3 + 208], %f16
	ldd		[%i3 + 216], %f18
	.word	0x80cd0444 !aes_dround01	%f20,%f4,%f2,%f0
	.word	0x84cd8464 !aes_dround23	%f22,%f4,%f2,%f2
	ldd		[%i3 + 224], %f20
	ldd		[%i3 + 232], %f22
	.word	0x88ce0440 !aes_dround01	%f24,%f0,%f2,%f4
	.word	0x84ce8460 !aes_dround23	%f26,%f0,%f2,%f2
	.word	0x80cf0444 !aes_dround01	%f28,%f4,%f2,%f0
	.word	0x84cf8464 !aes_dround23	%f30,%f4,%f2,%f2
	.word	0x88c84440 !aes_dround01	%f32,%f0,%f2,%f4
	.word	0x84c8c460 !aes_dround23	%f34,%f0,%f2,%f2
	.word	0x80c94444 !aes_dround01	%f36,%f4,%f2,%f0
	.word	0x84c9c464 !aes_dround23	%f38,%f4,%f2,%f2
	.word	0x88ca4440 !aes_dround01	%f40,%f0,%f2,%f4
	.word	0x84cac460 !aes_dround23	%f42,%f0,%f2,%f2
	.word	0x80cb4444 !aes_dround01	%f44,%f4,%f2,%f0
	.word	0x84cbc464 !aes_dround23	%f46,%f4,%f2,%f2
	.word	0x88cc4440 !aes_dround01	%f48,%f0,%f2,%f4
	.word	0x84ccc460 !aes_dround23	%f50,%f0,%f2,%f2
	.word	0x80cd4444 !aes_dround01	%f52,%f4,%f2,%f0
	.word	0x84cdc464 !aes_dround23	%f54,%f4,%f2,%f2
	.word	0x88ce4440 !aes_dround01	%f56,%f0,%f2,%f4
	.word	0x84cec460 !aes_dround23	%f58,%f0,%f2,%f2
	.word	0x80cf4444 !aes_dround01	%f60,%f4,%f2,%f0
	.word	0x84cfc464 !aes_dround23	%f62,%f4,%f2,%f2
	.word	0x88cc0440 !aes_dround01	%f16,%f0,%f2,%f4
	.word	0x84cc8460 !aes_dround23	%f18,%f0,%f2,%f2
	ldd		[%i3 + 16], %f16
	ldd		[%i3 + 24], %f18
	.word	0x80cd04c4 !aes_dround01_l	%f20,%f4,%f2,%f0
	.word	0x84cd84e4 !aes_dround23_l	%f22,%f4,%f2,%f2
	ldd		[%i3 + 32], %f20
	retl
	ldd		[%i3 + 40], %f22
.type	_aes256_decrypt_1x,#function
.size	_aes256_decrypt_1x,.-_aes256_decrypt_1x

.align	32
_aes256_decrypt_2x:
	.word	0x90cc0440 !aes_dround01	%f16,%f0,%f2,%f8
	.word	0x84cc8460 !aes_dround23	%f18,%f0,%f2,%f2
	.word	0x94cc0c44 !aes_dround01	%f16,%f4,%f6,%f10
	.word	0x8ccc8c64 !aes_dround23	%f18,%f4,%f6,%f6
	ldd		[%i3 + 208], %f16
	ldd		[%i3 + 216], %f18
	.word	0x80cd0448 !aes_dround01	%f20,%f8,%f2,%f0
	.word	0x84cd8468 !aes_dround23	%f22,%f8,%f2,%f2
	.word	0x88cd0c4a !aes_dround01	%f20,%f10,%f6,%f4
	.word	0x8ccd8c6a !aes_dround23	%f22,%f10,%f6,%f6
	ldd		[%i3 + 224], %f20
	ldd		[%i3 + 232], %f22
	.word	0x90ce0440 !aes_dround01	%f24,%f0,%f2,%f8
	.word	0x84ce8460 !aes_dround23	%f26,%f0,%f2,%f2
	.word	0x94ce0c44 !aes_dround01	%f24,%f4,%f6,%f10
	.word	0x8cce8c64 !aes_dround23	%f26,%f4,%f6,%f6
	.word	0x80cf0448 !aes_dround01	%f28,%f8,%f2,%f0
	.word	0x84cf8468 !aes_dround23	%f30,%f8,%f2,%f2
	.word	0x88cf0c4a !aes_dround01	%f28,%f10,%f6,%f4
	.word	0x8ccf8c6a !aes_dround23	%f30,%f10,%f6,%f6
	.word	0x90c84440 !aes_dround01	%f32,%f0,%f2,%f8
	.word	0x84c8c460 !aes_dround23	%f34,%f0,%f2,%f2
	.word	0x94c84c44 !aes_dround01	%f32,%f4,%f6,%f10
	.word	0x8cc8cc64 !aes_dround23	%f34,%f4,%f6,%f6
	.word	0x80c94448 !aes_dround01	%f36,%f8,%f2,%f0
	.word	0x84c9c468 !aes_dround23	%f38,%f8,%f2,%f2
	.word	0x88c94c4a !aes_dround01	%f36,%f10,%f6,%f4
	.word	0x8cc9cc6a !aes_dround23	%f38,%f10,%f6,%f6
	.word	0x90ca4440 !aes_dround01	%f40,%f0,%f2,%f8
	.word	0x84cac460 !aes_dround23	%f42,%f0,%f2,%f2
	.word	0x94ca4c44 !aes_dround01	%f40,%f4,%f6,%f10
	.word	0x8ccacc64 !aes_dround23	%f42,%f4,%f6,%f6
	.word	0x80cb4448 !aes_dround01	%f44,%f8,%f2,%f0
	.word	0x84cbc468 !aes_dround23	%f46,%f8,%f2,%f2
	.word	0x88cb4c4a !aes_dround01	%f44,%f10,%f6,%f4
	.word	0x8ccbcc6a !aes_dround23	%f46,%f10,%f6,%f6
	.word	0x90cc4440 !aes_dround01	%f48,%f0,%f2,%f8
	.word	0x84ccc460 !aes_dround23	%f50,%f0,%f2,%f2
	.word	0x94cc4c44 !aes_dround01	%f48,%f4,%f6,%f10
	.word	0x8ccccc64 !aes_dround23	%f50,%f4,%f6,%f6
	.word	0x80cd4448 !aes_dround01	%f52,%f8,%f2,%f0
	.word	0x84cdc468 !aes_dround23	%f54,%f8,%f2,%f2
	.word	0x88cd4c4a !aes_dround01	%f52,%f10,%f6,%f4
	.word	0x8ccdcc6a !aes_dround23	%f54,%f10,%f6,%f6
	.word	0x90ce4440 !aes_dround01	%f56,%f0,%f2,%f8
	.word	0x84cec460 !aes_dround23	%f58,%f0,%f2,%f2
	.word	0x94ce4c44 !aes_dround01	%f56,%f4,%f6,%f10
	.word	0x8ccecc64 !aes_dround23	%f58,%f4,%f6,%f6
	.word	0x80cf4448 !aes_dround01	%f60,%f8,%f2,%f0
	.word	0x84cfc468 !aes_dround23	%f62,%f8,%f2,%f2
	.word	0x88cf4c4a !aes_dround01	%f60,%f10,%f6,%f4
	.word	0x8ccfcc6a !aes_dround23	%f62,%f10,%f6,%f6
	.word	0x90cc0440 !aes_dround01	%f16,%f0,%f2,%f8
	.word	0x84cc8460 !aes_dround23	%f18,%f0,%f2,%f2
	.word	0x94cc0c44 !aes_dround01	%f16,%f4,%f6,%f10
	.word	0x8ccc8c64 !aes_dround23	%f18,%f4,%f6,%f6
	ldd		[%i3 + 16], %f16
	ldd		[%i3 + 24], %f18
	.word	0x80cd04c8 !aes_dround01_l	%f20,%f8,%f2,%f0
	.word	0x84cd84e8 !aes_dround23_l	%f22,%f8,%f2,%f2
	.word	0x88cd0cca !aes_dround01_l	%f20,%f10,%f6,%f4
	.word	0x8ccd8cea !aes_dround23_l	%f22,%f10,%f6,%f6
	ldd		[%i3 + 32], %f20
	retl
	ldd		[%i3 + 40], %f22
.type	_aes256_decrypt_2x,#function
.size	_aes256_decrypt_2x,.-_aes256_decrypt_2x

.align	32
_aes192_decrypt_1x:
	.word	0x88cc0440 !aes_dround01	%f16,%f0,%f2,%f4
	.word	0x84cc8460 !aes_dround23	%f18,%f0,%f2,%f2
	.word	0x80cd0444 !aes_dround01	%f20,%f4,%f2,%f0
	.word	0x84cd8464 !aes_dround23	%f22,%f4,%f2,%f2
	.word	0x88ce0440 !aes_dround01	%f24,%f0,%f2,%f4
	.word	0x84ce8460 !aes_dround23	%f26,%f0,%f2,%f2
	.word	0x80cf0444 !aes_dround01	%f28,%f4,%f2,%f0
	.word	0x84cf8464 !aes_dround23	%f30,%f4,%f2,%f2
	.word	0x88c84440 !aes_dround01	%f32,%f0,%f2,%f4
	.word	0x84c8c460 !aes_dround23	%f34,%f0,%f2,%f2
	.word	0x80c94444 !aes_dround01	%f36,%f4,%f2,%f0
	.word	0x84c9c464 !aes_dround23	%f38,%f4,%f2,%f2
	.word	0x88ca4440 !aes_dround01	%f40,%f0,%f2,%f4
	.word	0x84cac460 !aes_dround23	%f42,%f0,%f2,%f2
	.word	0x80cb4444 !aes_dround01	%f44,%f4,%f2,%f0
	.word	0x84cbc464 !aes_dround23	%f46,%f4,%f2,%f2
	.word	0x88cc4440 !aes_dround01	%f48,%f0,%f2,%f4
	.word	0x84ccc460 !aes_dround23	%f50,%f0,%f2,%f2
	.word	0x80cd4444 !aes_dround01	%f52,%f4,%f2,%f0
	.word	0x84cdc464 !aes_dround23	%f54,%f4,%f2,%f2
	.word	0x88ce4440 !aes_dround01	%f56,%f0,%f2,%f4
	.word	0x84cec460 !aes_dround23	%f58,%f0,%f2,%f2
	.word	0x80cf44c4 !aes_dround01_l	%f60,%f4,%f2,%f0
	retl
	.word	0x84cfc4e4 !aes_dround23_l	%f62,%f4,%f2,%f2
.type	_aes192_decrypt_1x,#function
.size	_aes192_decrypt_1x,.-_aes192_decrypt_1x

.align	32
_aes192_decrypt_2x:
	.word	0x90cc0440 !aes_dround01	%f16,%f0,%f2,%f8
	.word	0x84cc8460 !aes_dround23	%f18,%f0,%f2,%f2
	.word	0x94cc0c44 !aes_dround01	%f16,%f4,%f6,%f10
	.word	0x8ccc8c64 !aes_dround23	%f18,%f4,%f6,%f6
	.word	0x80cd0448 !aes_dround01	%f20,%f8,%f2,%f0
	.word	0x84cd8468 !aes_dround23	%f22,%f8,%f2,%f2
	.word	0x88cd0c4a !aes_dround01	%f20,%f10,%f6,%f4
	.word	0x8ccd8c6a !aes_dround23	%f22,%f10,%f6,%f6
	.word	0x90ce0440 !aes_dround01	%f24,%f0,%f2,%f8
	.word	0x84ce8460 !aes_dround23	%f26,%f0,%f2,%f2
	.word	0x94ce0c44 !aes_dround01	%f24,%f4,%f6,%f10
	.word	0x8cce8c64 !aes_dround23	%f26,%f4,%f6,%f6
	.word	0x80cf0448 !aes_dround01	%f28,%f8,%f2,%f0
	.word	0x84cf8468 !aes_dround23	%f30,%f8,%f2,%f2
	.word	0x88cf0c4a !aes_dround01	%f28,%f10,%f6,%f4
	.word	0x8ccf8c6a !aes_dround23	%f30,%f10,%f6,%f6
	.word	0x90c84440 !aes_dround01	%f32,%f0,%f2,%f8
	.word	0x84c8c460 !aes_dround23	%f34,%f0,%f2,%f2
	.word	0x94c84c44 !aes_dround01	%f32,%f4,%f6,%f10
	.word	0x8cc8cc64 !aes_dround23	%f34,%f4,%f6,%f6
	.word	0x80c94448 !aes_dround01	%f36,%f8,%f2,%f0
	.word	0x84c9c468 !aes_dround23	%f38,%f8,%f2,%f2
	.word	0x88c94c4a !aes_dround01	%f36,%f10,%f6,%f4
	.word	0x8cc9cc6a !aes_dround23	%f38,%f10,%f6,%f6
	.word	0x90ca4440 !aes_dround01	%f40,%f0,%f2,%f8
	.word	0x84cac460 !aes_dround23	%f42,%f0,%f2,%f2
	.word	0x94ca4c44 !aes_dround01	%f40,%f4,%f6,%f10
	.word	0x8ccacc64 !aes_dround23	%f42,%f4,%f6,%f6
	.word	0x80cb4448 !aes_dround01	%f44,%f8,%f2,%f0
	.word	0x84cbc468 !aes_dround23	%f46,%f8,%f2,%f2
	.word	0x88cb4c4a !aes_dround01	%f44,%f10,%f6,%f4
	.word	0x8ccbcc6a !aes_dround23	%f46,%f10,%f6,%f6
	.word	0x90cc4440 !aes_dround01	%f48,%f0,%f2,%f8
	.word	0x84ccc460 !aes_dround23	%f50,%f0,%f2,%f2
	.word	0x94cc4c44 !aes_dround01	%f48,%f4,%f6,%f10
	.word	0x8ccccc64 !aes_dround23	%f50,%f4,%f6,%f6
	.word	0x80cd4448 !aes_dround01	%f52,%f8,%f2,%f0
	.word	0x84cdc468 !aes_dround23	%f54,%f8,%f2,%f2
	.word	0x88cd4c4a !aes_dround01	%f52,%f10,%f6,%f4
	.word	0x8ccdcc6a !aes_dround23	%f54,%f10,%f6,%f6
	.word	0x90ce4440 !aes_dround01	%f56,%f0,%f2,%f8
	.word	0x84cec460 !aes_dround23	%f58,%f0,%f2,%f2
	.word	0x94ce4c44 !aes_dround01	%f56,%f4,%f6,%f10
	.word	0x8ccecc64 !aes_dround23	%f58,%f4,%f6,%f6
	.word	0x80cf44c8 !aes_dround01_l	%f60,%f8,%f2,%f0
	.word	0x84cfc4e8 !aes_dround23_l	%f62,%f8,%f2,%f2
	.word	0x88cf4cca !aes_dround01_l	%f60,%f10,%f6,%f4
	retl
	.word	0x8ccfccea !aes_dround23_l	%f62,%f10,%f6,%f6
.type	_aes192_decrypt_2x,#function
.size	_aes192_decrypt_2x,.-_aes192_decrypt_2x
.asciz	"AES for SPARC T4, David S. Miller, Andy Polyakov"
.align	4
