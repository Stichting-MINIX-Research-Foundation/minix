.text

.globl	cmll_t4_encrypt
.align	32
cmll_t4_encrypt:
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
	ld		[%o2 + 272], %o3	! grandRounds, 3 or 4
	ldd		[%o2 + 16], %f12
	ldd		[%o2 + 24], %f14
	xor		%g4, %o4, %o4
	xor		%g5, %o5, %o5
	ldd		[%o2 + 32], %f16
	ldd		[%o2 + 40], %f18
	.word	0x81b0230c !movxtod	%o4,%f0
	.word	0x85b0230d !movxtod	%o5,%f2
	ldd		[%o2 + 48], %f20
	ldd		[%o2 + 56], %f22
	sub		%o3, 1, %o3
	ldd		[%o2 + 64], %f24
	ldd		[%o2 + 72], %f26
	add		%o2, 80, %o2

.Lenc:
	.word	0x84cb0182 !camellia_f	%f12,%f2,%f0,%f2
	ldd		[%o2 + 0], %f12
	sub		%o3,1,%o3
	.word	0x80cb8580 !camellia_f	%f14,%f0,%f2,%f0
	ldd		[%o2 + 8], %f14
	.word	0x84cc0182 !camellia_f	%f16,%f2,%f0,%f2
	ldd		[%o2 + 16], %f16
	.word	0x80cc8580 !camellia_f	%f18,%f0,%f2,%f0
	ldd		[%o2 + 24], %f18
	.word	0x84cd0182 !camellia_f	%f20,%f2,%f0,%f2
	ldd		[%o2 + 32], %f20
	.word	0x80cd8580 !camellia_f	%f22,%f0,%f2,%f0
	ldd		[%o2 + 40], %f22
	.word	0x81b62780 !camellia_fl	%f24,%f0,%f0
	ldd		[%o2 + 48], %f24
	.word	0x85b6a7a2 !camellia_fli	%f26,%f2,%f2
	ldd		[%o2 + 56], %f26
	brnz,pt		%o3, .Lenc
	add		%o2, 64, %o2

	andcc		%o1, 7, %o4		! is output aligned?
	.word	0x84cb0182 !camellia_f	%f12,%f2,%f0,%f2
	.word	0x80cb8580 !camellia_f	%f14,%f0,%f2,%f0
	.word	0x84cc0182 !camellia_f	%f16,%f2,%f0,%f2
	.word	0x80cc8580 !camellia_f	%f18,%f0,%f2,%f0
	.word	0x88cd0182 !camellia_f	%f20,%f2,%f0,%f4
	.word	0x84cd8980 !camellia_f	%f22,%f0,%f4,%f2
	.word	0x81b60d84 !fxor	%f24,%f4,%f0
	.word	0x85b68d82 !fxor	%f26,%f2,%f2

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
.type	cmll_t4_encrypt,#function
.size	cmll_t4_encrypt,.-cmll_t4_encrypt

.globl	cmll_t4_decrypt
.align	32
cmll_t4_decrypt:
	ld		[%o2 + 272], %o3	! grandRounds, 3 or 4
	andcc		%o0, 7, %g1		! is input aligned?
	andn		%o0, 7, %o0

	sll		%o3, 6, %o3
	add		%o3, %o2, %o2

	ldx		[%o0 + 0], %o4
	bz,pt		%icc, 1f
	ldx		[%o0 + 8], %o5
	ldx		[%o0 + 16], %o0
	sll		%g1, 3, %g1
	sub		%g0, %g1, %g4
	sllx		%o4, %g1, %o4
	sllx		%o5, %g1, %g1
	srlx		%o5, %g4, %o5
	srlx		%o0, %g4, %g4
	or		%o5, %o4, %o4
	or		%g4, %g1, %o5
1:
	ldx		[%o2 + 0], %g4
	ldx		[%o2 + 8], %g5
	ldd		[%o2 - 8], %f12
	ldd		[%o2 - 16], %f14
	xor		%g4, %o4, %o4
	xor		%g5, %o5, %o5
	ldd		[%o2 - 24], %f16
	ldd		[%o2 - 32], %f18
	.word	0x81b0230c !movxtod	%o4,%f0
	.word	0x85b0230d !movxtod	%o5,%f2
	ldd		[%o2 - 40], %f20
	ldd		[%o2 - 48], %f22
	sub		%o3, 64, %o3
	ldd		[%o2 - 56], %f24
	ldd		[%o2 - 64], %f26
	sub		%o2, 64, %o2

.Ldec:
	.word	0x84cb0182 !camellia_f	%f12,%f2,%f0,%f2
	ldd		[%o2 - 8], %f12
	sub		%o3, 64, %o3
	.word	0x80cb8580 !camellia_f	%f14,%f0,%f2,%f0
	ldd		[%o2 - 16], %f14
	.word	0x84cc0182 !camellia_f	%f16,%f2,%f0,%f2
	ldd		[%o2 - 24], %f16
	.word	0x80cc8580 !camellia_f	%f18,%f0,%f2,%f0
	ldd		[%o2 - 32], %f18
	.word	0x84cd0182 !camellia_f	%f20,%f2,%f0,%f2
	ldd		[%o2 - 40], %f20
	.word	0x80cd8580 !camellia_f	%f22,%f0,%f2,%f0
	ldd		[%o2 - 48], %f22
	.word	0x81b62780 !camellia_fl	%f24,%f0,%f0
	ldd		[%o2 - 56], %f24
	.word	0x85b6a7a2 !camellia_fli	%f26,%f2,%f2
	ldd		[%o2 - 64], %f26
	brnz,pt		%o3, .Ldec
	sub		%o2, 64, %o2

	andcc		%o1, 7, %o4		! is output aligned?
	.word	0x84cb0182 !camellia_f	%f12,%f2,%f0,%f2
	.word	0x80cb8580 !camellia_f	%f14,%f0,%f2,%f0
	.word	0x84cc0182 !camellia_f	%f16,%f2,%f0,%f2
	.word	0x80cc8580 !camellia_f	%f18,%f0,%f2,%f0
	.word	0x88cd0182 !camellia_f	%f20,%f2,%f0,%f4
	.word	0x84cd8980 !camellia_f	%f22,%f0,%f4,%f2
	.word	0x81b68d84 !fxor	%f26,%f4,%f0
	.word	0x85b60d82 !fxor	%f24,%f2,%f2

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
.type	cmll_t4_decrypt,#function
.size	cmll_t4_decrypt,.-cmll_t4_decrypt
.globl	cmll_t4_set_key
.align	32
cmll_t4_set_key:
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
	b		.L256aligned
	.word	0x8db18908 !faligndata	%f6,%f8,%f6

.align	16
.L192:
	brz,a,pt	%o3, .L256aligned
	.word	0x8db00cc4 !fnot2	%f0,%f4,%f6

	ldd		[%o0 + 24], %f6
	nop
	.word	0x81b00902 !faligndata	%f0,%f2,%f0
	.word	0x85b08904 !faligndata	%f2,%f4,%f2
	.word	0x89b10906 !faligndata	%f4,%f6,%f4
	.word	0x8db00cc4 !fnot2	%f0,%f4,%f6

.L256aligned:
	std		%f0, [%o2 + 0]		! k[0, 1]
	.word	0xb9b00f00 !fsrc2	%f0,%f0,%f28
	std		%f2, [%o2 + 8]		! k[2, 3]
	.word	0xbdb00f02 !fsrc2	%f0,%f2,%f30
	.word	0x81b10d80 !fxor	%f4,%f0,%f0
	b		.L128key
	.word	0x85b18d82 !fxor	%f6,%f2,%f2

.align	16
.L128:
	brz,pt		%o3, .L128aligned
	nop

	ldd		[%o0 + 16], %f4
	nop
	.word	0x81b00902 !faligndata	%f0,%f2,%f0
	.word	0x85b08904 !faligndata	%f2,%f4,%f2

.L128aligned:
	std		%f0, [%o2 + 0]		! k[0, 1]
	.word	0xb9b00f00 !fsrc2	%f0,%f0,%f28
	std		%f2, [%o2 + 8]		! k[2, 3]
	.word	0xbdb00f02 !fsrc2	%f0,%f2,%f30

.L128key:
	mov		%o7, %o5
1:	call		.+8
	add		%o7, SIGMA-1b, %o4
	mov		%o5, %o7

	ldd		[%o4 + 0], %f16
	ldd		[%o4 + 8], %f18
	ldd		[%o4 + 16], %f20
	ldd		[%o4 + 24], %f22

	.word	0x84cc0182 !camellia_f	%f16,%f2,%f0,%f2
	.word	0x80cc8580 !camellia_f	%f18,%f0,%f2,%f0
	.word	0x81b70d80 !fxor	%f28,%f0,%f0
	.word	0x85b78d82 !fxor	%f30,%f2,%f2
	.word	0x84cd0182 !camellia_f	%f20,%f2,%f0,%f2
	.word	0x80cd8580 !camellia_f	%f22,%f0,%f2,%f0

	bge,pn		%icc, .L256key
	nop
	std	%f0, [%o2 + 0x10]	! k[ 4,  5]
	std	%f2, [%o2 + 0x18]	! k[ 6,  7]

	.word	0x99b02200 !movdtox	%f0,%o4
	.word	0x9bb02202 !movdtox	%f2,%o5
	srlx	%o4, 64-15, %g4
	sllx	%o4, 15, %o4
	srlx	%o5, 64-15, %g5
	sllx	%o5, 15, %o5
	or	%o4, %g5, %o4
	or	%o5, %g4, %o5
	stx	%o4, [%o2 + 0x30]	! k[12, 13]
	stx	%o5, [%o2 + 0x38]	! k[14, 15]
	srlx	%o4, 64-15, %g4
	sllx	%o4, 15, %o4
	srlx	%o5, 64-15, %g5
	sllx	%o5, 15, %o5
	or	%o4, %g5, %o4
	or	%o5, %g4, %o5
	stx	%o4, [%o2 + 0x40]	! k[16, 17]
	stx	%o5, [%o2 + 0x48]	! k[18, 19]
	srlx	%o4, 64-15, %g4
	sllx	%o4, 15, %o4
	srlx	%o5, 64-15, %g5
	sllx	%o5, 15, %o5
	or	%o4, %g5, %o4
	or	%o5, %g4, %o5
	stx	%o4, [%o2 + 0x60]	! k[24, 25]
	srlx	%o4, 64-15, %g4
	sllx	%o4, 15, %o4
	srlx	%o5, 64-15, %g5
	sllx	%o5, 15, %o5
	or	%o4, %g5, %o4
	or	%o5, %g4, %o5
	stx	%o4, [%o2 + 0x70]	! k[28, 29]
	stx	%o5, [%o2 + 0x78]	! k[30, 31]
	srlx	%o4, 64-34, %g4
	sllx	%o4, 34, %o4
	srlx	%o5, 64-34, %g5
	sllx	%o5, 34, %o5
	or	%o4, %g5, %o4
	or	%o5, %g4, %o5
	stx	%o4, [%o2 + 0xa0]	! k[40, 41]
	stx	%o5, [%o2 + 0xa8]	! k[42, 43]
	srlx	%o4, 64-17, %g4
	sllx	%o4, 17, %o4
	srlx	%o5, 64-17, %g5
	sllx	%o5, 17, %o5
	or	%o4, %g5, %o4
	or	%o5, %g4, %o5
	stx	%o4, [%o2 + 0xc0]	! k[48, 49]
	stx	%o5, [%o2 + 0xc8]	! k[50, 51]

	.word	0x99b0221c !movdtox	%f28,%o4		! k[ 0,  1]
	.word	0x9bb0221e !movdtox	%f30,%o5		! k[ 2,  3]
	srlx	%o4, 64-15, %g4
	sllx	%o4, 15, %o4
	srlx	%o5, 64-15, %g5
	sllx	%o5, 15, %o5
	or	%o4, %g5, %o4
	or	%o5, %g4, %o5
	stx	%o4, [%o2 + 0x20]	! k[ 8,  9]
	stx	%o5, [%o2 + 0x28]	! k[10, 11]
	srlx	%o4, 64-30, %g4
	sllx	%o4, 30, %o4
	srlx	%o5, 64-30, %g5
	sllx	%o5, 30, %o5
	or	%o4, %g5, %o4
	or	%o5, %g4, %o5
	stx	%o4, [%o2 + 0x50]	! k[20, 21]
	stx	%o5, [%o2 + 0x58]	! k[22, 23]
	srlx	%o4, 64-15, %g4
	sllx	%o4, 15, %o4
	srlx	%o5, 64-15, %g5
	sllx	%o5, 15, %o5
	or	%o4, %g5, %o4
	or	%o5, %g4, %o5
	stx	%o5, [%o2 + 0x68]	! k[26, 27]
	srlx	%o4, 64-17, %g4
	sllx	%o4, 17, %o4
	srlx	%o5, 64-17, %g5
	sllx	%o5, 17, %o5
	or	%o4, %g5, %o4
	or	%o5, %g4, %o5
	stx	%o4, [%o2 + 0x80]	! k[32, 33]
	stx	%o5, [%o2 + 0x88]	! k[34, 35]
	srlx	%o4, 64-17, %g4
	sllx	%o4, 17, %o4
	srlx	%o5, 64-17, %g5
	sllx	%o5, 17, %o5
	or	%o4, %g5, %o4
	or	%o5, %g4, %o5
	stx	%o4, [%o2 + 0x90]	! k[36, 37]
	stx	%o5, [%o2 + 0x98]	! k[38, 39]
	srlx	%o4, 64-17, %g4
	sllx	%o4, 17, %o4
	srlx	%o5, 64-17, %g5
	sllx	%o5, 17, %o5
	or	%o4, %g5, %o4
	or	%o5, %g4, %o5
	stx	%o4, [%o2 + 0xb0]	! k[44, 45]
	stx	%o5, [%o2 + 0xb8]	! k[46, 47]

	mov		3, %o3
	st		%o3, [%o2 + 0x110]
	retl
	xor		%o0, %o0, %o0

.align	16
.L256key:
	ldd		[%o4 + 32], %f24
	ldd		[%o4 + 40], %f26

	std		%f0, [%o2 + 0x30]	! k[12, 13]
	std		%f2, [%o2 + 0x38]	! k[14, 15]

	.word	0x81b10d80 !fxor	%f4,%f0,%f0
	.word	0x85b18d82 !fxor	%f6,%f2,%f2
	.word	0x84ce0182 !camellia_f	%f24,%f2,%f0,%f2
	.word	0x80ce8580 !camellia_f	%f26,%f0,%f2,%f0

	std	%f0, [%o2 + 0x10]	! k[ 4,  5]
	std	%f2, [%o2 + 0x18]	! k[ 6,  7]

	.word	0x99b02200 !movdtox	%f0,%o4
	.word	0x9bb02202 !movdtox	%f2,%o5
	srlx	%o4, 64-30, %g4
	sllx	%o4, 30, %o4
	srlx	%o5, 64-30, %g5
	sllx	%o5, 30, %o5
	or	%o4, %g5, %o4
	or	%o5, %g4, %o5
	stx	%o4, [%o2 + 0x50]	! k[20, 21]
	stx	%o5, [%o2 + 0x58]	! k[22, 23]
	srlx	%o4, 64-30, %g4
	sllx	%o4, 30, %o4
	srlx	%o5, 64-30, %g5
	sllx	%o5, 30, %o5
	or	%o4, %g5, %o4
	or	%o5, %g4, %o5
	stx	%o4, [%o2 + 0xa0]	! k[40, 41]
	stx	%o5, [%o2 + 0xa8]	! k[42, 43]
	srlx	%o4, 64-51, %g4
	sllx	%o4, 51, %o4
	srlx	%o5, 64-51, %g5
	sllx	%o5, 51, %o5
	or	%o4, %g5, %o4
	or	%o5, %g4, %o5
	stx	%o4, [%o2 + 0x100]	! k[64, 65]
	stx	%o5, [%o2 + 0x108]	! k[66, 67]

	.word	0x99b02204 !movdtox	%f4,%o4		! k[ 8,  9]
	.word	0x9bb02206 !movdtox	%f6,%o5		! k[10, 11]
	srlx	%o4, 64-15, %g4
	sllx	%o4, 15, %o4
	srlx	%o5, 64-15, %g5
	sllx	%o5, 15, %o5
	or	%o4, %g5, %o4
	or	%o5, %g4, %o5
	stx	%o4, [%o2 + 0x20]	! k[ 8,  9]
	stx	%o5, [%o2 + 0x28]	! k[10, 11]
	srlx	%o4, 64-15, %g4
	sllx	%o4, 15, %o4
	srlx	%o5, 64-15, %g5
	sllx	%o5, 15, %o5
	or	%o4, %g5, %o4
	or	%o5, %g4, %o5
	stx	%o4, [%o2 + 0x40]	! k[16, 17]
	stx	%o5, [%o2 + 0x48]	! k[18, 19]
	srlx	%o4, 64-30, %g4
	sllx	%o4, 30, %o4
	srlx	%o5, 64-30, %g5
	sllx	%o5, 30, %o5
	or	%o4, %g5, %o4
	or	%o5, %g4, %o5
	stx	%o4, [%o2 + 0x90]	! k[36, 37]
	stx	%o5, [%o2 + 0x98]	! k[38, 39]
	srlx	%o4, 64-34, %g4
	sllx	%o4, 34, %o4
	srlx	%o5, 64-34, %g5
	sllx	%o5, 34, %o5
	or	%o4, %g5, %o4
	or	%o5, %g4, %o5
	stx	%o4, [%o2 + 0xd0]	! k[52, 53]
	stx	%o5, [%o2 + 0xd8]	! k[54, 55]
	ldx	[%o2 + 0x30], %o4	! k[12, 13]
	ldx	[%o2 + 0x38], %o5	! k[14, 15]
	srlx	%o4, 64-15, %g4
	sllx	%o4, 15, %o4
	srlx	%o5, 64-15, %g5
	sllx	%o5, 15, %o5
	or	%o4, %g5, %o4
	or	%o5, %g4, %o5
	stx	%o4, [%o2 + 0x30]	! k[12, 13]
	stx	%o5, [%o2 + 0x38]	! k[14, 15]
	srlx	%o4, 64-30, %g4
	sllx	%o4, 30, %o4
	srlx	%o5, 64-30, %g5
	sllx	%o5, 30, %o5
	or	%o4, %g5, %o4
	or	%o5, %g4, %o5
	stx	%o4, [%o2 + 0x70]	! k[28, 29]
	stx	%o5, [%o2 + 0x78]	! k[30, 31]
	srlx	%o4, 32, %g4
	srlx	%o5, 32, %g5
	st	%o4, [%o2 + 0xc0]	! k[48]
	st	%g5, [%o2 + 0xc4]	! k[49]
	st	%o5, [%o2 + 0xc8]	! k[50]
	st	%g4, [%o2 + 0xcc]	! k[51]
	srlx	%o4, 64-49, %g4
	sllx	%o4, 49, %o4
	srlx	%o5, 64-49, %g5
	sllx	%o5, 49, %o5
	or	%o4, %g5, %o4
	or	%o5, %g4, %o5
	stx	%o4, [%o2 + 0xe0]	! k[56, 57]
	stx	%o5, [%o2 + 0xe8]	! k[58, 59]

	.word	0x99b0221c !movdtox	%f28,%o4		! k[ 0,  1]
	.word	0x9bb0221e !movdtox	%f30,%o5		! k[ 2,  3]
	srlx	%o4, 64-45, %g4
	sllx	%o4, 45, %o4
	srlx	%o5, 64-45, %g5
	sllx	%o5, 45, %o5
	or	%o4, %g5, %o4
	or	%o5, %g4, %o5
	stx	%o4, [%o2 + 0x60]	! k[24, 25]
	stx	%o5, [%o2 + 0x68]	! k[26, 27]
	srlx	%o4, 64-15, %g4
	sllx	%o4, 15, %o4
	srlx	%o5, 64-15, %g5
	sllx	%o5, 15, %o5
	or	%o4, %g5, %o4
	or	%o5, %g4, %o5
	stx	%o4, [%o2 + 0x80]	! k[32, 33]
	stx	%o5, [%o2 + 0x88]	! k[34, 35]
	srlx	%o4, 64-17, %g4
	sllx	%o4, 17, %o4
	srlx	%o5, 64-17, %g5
	sllx	%o5, 17, %o5
	or	%o4, %g5, %o4
	or	%o5, %g4, %o5
	stx	%o4, [%o2 + 0xb0]	! k[44, 45]
	stx	%o5, [%o2 + 0xb8]	! k[46, 47]
	srlx	%o4, 64-34, %g4
	sllx	%o4, 34, %o4
	srlx	%o5, 64-34, %g5
	sllx	%o5, 34, %o5
	or	%o4, %g5, %o4
	or	%o5, %g4, %o5
	stx	%o4, [%o2 + 0xf0]	! k[60, 61]
	stx	%o5, [%o2 + 0xf8]	! k[62, 63]

	mov		4, %o3
	st		%o3, [%o2 + 0x110]
	retl
	xor		%o0, %o0, %o0
.type	cmll_t4_set_key,#function
.size	cmll_t4_set_key,.-cmll_t4_set_key
.align	32
SIGMA:
	.long	0xa09e667f, 0x3bcc908b, 0xb67ae858, 0x4caa73b2
	.long	0xc6ef372f, 0xe94f82be, 0x54ff53a5, 0xf1d36f1c
	.long	0x10e527fa, 0xde682d1d, 0xb05688c2, 0xb3e6c1fd
.type	SIGMA,#object
.size	SIGMA,.-SIGMA
.asciz	"Camellia for SPARC T4, David S. Miller, Andy Polyakov"
.align	32
_cmll128_load_enckey:
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
.type	_cmll128_load_enckey,#function
.size	_cmll128_load_enckey,.-_cmll128_load_enckey
_cmll256_load_enckey=_cmll128_load_enckey

.align	32
_cmll256_load_deckey:
	ldd		[%i3 + 64], %f62
	ldd		[%i3 + 72], %f60
	b		.Load_deckey
	add		%i3, 64, %i3
_cmll128_load_deckey:
	ldd		[%i3 + 0], %f60
	ldd		[%i3 + 8], %f62
.Load_deckey:
	ldd		[%i3 + 16], %f58
	ldd		[%i3 + 24], %f56
	ldd		[%i3 + 32], %f54
	ldd		[%i3 + 40], %f52
	ldd		[%i3 + 48], %f50
	ldd		[%i3 + 56], %f48
	ldd		[%i3 + 64], %f46
	ldd		[%i3 + 72], %f44
	ldd		[%i3 + 80], %f42
	ldd		[%i3 + 88], %f40
	ldd		[%i3 + 96], %f38
	ldd		[%i3 + 104], %f36
	ldd		[%i3 + 112], %f34
	ldd		[%i3 + 120], %f32
	ldd		[%i3 + 128], %f30
	ldd		[%i3 + 136], %f28
	ldd		[%i3 + 144], %f26
	ldd		[%i3 + 152], %f24
	ldd		[%i3 + 160], %f22
	ldd		[%i3 + 168], %f20
	ldd		[%i3 + 176], %f18
	ldd		[%i3 + 184], %f16
	ldx		[%i3 + 192], %g4
	retl
	ldx		[%i3 + 200], %g5
.type	_cmll256_load_deckey,#function
.size	_cmll256_load_deckey,.-_cmll256_load_deckey

.align	32
_cmll128_encrypt_1x:
	.word	0x84cc0182 !camellia_f	%f16,%f2,%f0,%f2
	.word	0x80cc8580 !camellia_f	%f18,%f0,%f2,%f0
	.word	0x84cd0182 !camellia_f	%f20,%f2,%f0,%f2
	.word	0x80cd8580 !camellia_f	%f22,%f0,%f2,%f0
	.word	0x84ce0182 !camellia_f	%f24,%f2,%f0,%f2
	.word	0x80ce8580 !camellia_f	%f26,%f0,%f2,%f0
	.word	0x81b72780 !camellia_fl	%f28,%f0,%f0
	.word	0x85b7a7a2 !camellia_fli	%f30,%f2,%f2
	.word	0x84c84182 !camellia_f	%f32,%f2,%f0,%f2
	.word	0x80c8c580 !camellia_f	%f34,%f0,%f2,%f0
	.word	0x84c94182 !camellia_f	%f36,%f2,%f0,%f2
	.word	0x80c9c580 !camellia_f	%f38,%f0,%f2,%f0
	.word	0x84ca4182 !camellia_f	%f40,%f2,%f0,%f2
	.word	0x80cac580 !camellia_f	%f42,%f0,%f2,%f0
	.word	0x81b36780 !camellia_fl	%f44,%f0,%f0
	.word	0x85b3e7a2 !camellia_fli	%f46,%f2,%f2
	.word	0x84cc4182 !camellia_f	%f48,%f2,%f0,%f2
	.word	0x80ccc580 !camellia_f	%f50,%f0,%f2,%f0
	.word	0x84cd4182 !camellia_f	%f52,%f2,%f0,%f2
	.word	0x80cdc580 !camellia_f	%f54,%f0,%f2,%f0
	.word	0x88ce4182 !camellia_f	%f56,%f2,%f0,%f4
	.word	0x84cec980 !camellia_f	%f58,%f0,%f4,%f2
	.word	0x81b74d84 !fxor	%f60,%f4,%f0
	retl
	.word	0x85b7cd82 !fxor	%f62,%f2,%f2
.type	_cmll128_encrypt_1x,#function
.size	_cmll128_encrypt_1x,.-_cmll128_encrypt_1x
_cmll128_decrypt_1x=_cmll128_encrypt_1x

.align	32
_cmll128_encrypt_2x:
	.word	0x84cc0182 !camellia_f	%f16,%f2,%f0,%f2
	.word	0x8ccc0986 !camellia_f	%f16,%f6,%f4,%f6
	.word	0x80cc8580 !camellia_f	%f18,%f0,%f2,%f0
	.word	0x88cc8d84 !camellia_f	%f18,%f4,%f6,%f4
	.word	0x84cd0182 !camellia_f	%f20,%f2,%f0,%f2
	.word	0x8ccd0986 !camellia_f	%f20,%f6,%f4,%f6
	.word	0x80cd8580 !camellia_f	%f22,%f0,%f2,%f0
	.word	0x88cd8d84 !camellia_f	%f22,%f4,%f6,%f4
	.word	0x84ce0182 !camellia_f	%f24,%f2,%f0,%f2
	.word	0x8cce0986 !camellia_f	%f24,%f6,%f4,%f6
	.word	0x80ce8580 !camellia_f	%f26,%f0,%f2,%f0
	.word	0x88ce8d84 !camellia_f	%f26,%f4,%f6,%f4
	.word	0x81b72780 !camellia_fl	%f28,%f0,%f0
	.word	0x89b72784 !camellia_fl	%f28,%f4,%f4
	.word	0x85b7a7a2 !camellia_fli	%f30,%f2,%f2
	.word	0x8db7a7a6 !camellia_fli	%f30,%f6,%f6
	.word	0x84c84182 !camellia_f	%f32,%f2,%f0,%f2
	.word	0x8cc84986 !camellia_f	%f32,%f6,%f4,%f6
	.word	0x80c8c580 !camellia_f	%f34,%f0,%f2,%f0
	.word	0x88c8cd84 !camellia_f	%f34,%f4,%f6,%f4
	.word	0x84c94182 !camellia_f	%f36,%f2,%f0,%f2
	.word	0x8cc94986 !camellia_f	%f36,%f6,%f4,%f6
	.word	0x80c9c580 !camellia_f	%f38,%f0,%f2,%f0
	.word	0x88c9cd84 !camellia_f	%f38,%f4,%f6,%f4
	.word	0x84ca4182 !camellia_f	%f40,%f2,%f0,%f2
	.word	0x8cca4986 !camellia_f	%f40,%f6,%f4,%f6
	.word	0x80cac580 !camellia_f	%f42,%f0,%f2,%f0
	.word	0x88cacd84 !camellia_f	%f42,%f4,%f6,%f4
	.word	0x81b36780 !camellia_fl	%f44,%f0,%f0
	.word	0x89b36784 !camellia_fl	%f44,%f4,%f4
	.word	0x85b3e7a2 !camellia_fli	%f46,%f2,%f2
	.word	0x8db3e7a6 !camellia_fli	%f46,%f6,%f6
	.word	0x84cc4182 !camellia_f	%f48,%f2,%f0,%f2
	.word	0x8ccc4986 !camellia_f	%f48,%f6,%f4,%f6
	.word	0x80ccc580 !camellia_f	%f50,%f0,%f2,%f0
	.word	0x88cccd84 !camellia_f	%f50,%f4,%f6,%f4
	.word	0x84cd4182 !camellia_f	%f52,%f2,%f0,%f2
	.word	0x8ccd4986 !camellia_f	%f52,%f6,%f4,%f6
	.word	0x80cdc580 !camellia_f	%f54,%f0,%f2,%f0
	.word	0x88cdcd84 !camellia_f	%f54,%f4,%f6,%f4
	.word	0x90ce4182 !camellia_f	%f56,%f2,%f0,%f8
	.word	0x94ce4986 !camellia_f	%f56,%f6,%f4,%f10
	.word	0x84ced180 !camellia_f	%f58,%f0,%f8,%f2
	.word	0x8cced584 !camellia_f	%f58,%f4,%f10,%f6
	.word	0x81b74d88 !fxor	%f60,%f8,%f0
	.word	0x89b74d8a !fxor	%f60,%f10,%f4
	.word	0x85b7cd82 !fxor	%f62,%f2,%f2
	retl
	.word	0x8db7cd86 !fxor	%f62,%f6,%f6
.type	_cmll128_encrypt_2x,#function
.size	_cmll128_encrypt_2x,.-_cmll128_encrypt_2x
_cmll128_decrypt_2x=_cmll128_encrypt_2x

.align	32
_cmll256_encrypt_1x:
	.word	0x84cc0182 !camellia_f	%f16,%f2,%f0,%f2
	.word	0x80cc8580 !camellia_f	%f18,%f0,%f2,%f0
	ldd		[%i3 + 208], %f16
	ldd		[%i3 + 216], %f18
	.word	0x84cd0182 !camellia_f	%f20,%f2,%f0,%f2
	.word	0x80cd8580 !camellia_f	%f22,%f0,%f2,%f0
	ldd		[%i3 + 224], %f20
	ldd		[%i3 + 232], %f22
	.word	0x84ce0182 !camellia_f	%f24,%f2,%f0,%f2
	.word	0x80ce8580 !camellia_f	%f26,%f0,%f2,%f0
	ldd		[%i3 + 240], %f24
	ldd		[%i3 + 248], %f26
	.word	0x81b72780 !camellia_fl	%f28,%f0,%f0
	.word	0x85b7a7a2 !camellia_fli	%f30,%f2,%f2
	ldd		[%i3 + 256], %f28
	ldd		[%i3 + 264], %f30
	.word	0x84c84182 !camellia_f	%f32,%f2,%f0,%f2
	.word	0x80c8c580 !camellia_f	%f34,%f0,%f2,%f0
	.word	0x84c94182 !camellia_f	%f36,%f2,%f0,%f2
	.word	0x80c9c580 !camellia_f	%f38,%f0,%f2,%f0
	.word	0x84ca4182 !camellia_f	%f40,%f2,%f0,%f2
	.word	0x80cac580 !camellia_f	%f42,%f0,%f2,%f0
	.word	0x81b36780 !camellia_fl	%f44,%f0,%f0
	.word	0x85b3e7a2 !camellia_fli	%f46,%f2,%f2
	.word	0x84cc4182 !camellia_f	%f48,%f2,%f0,%f2
	.word	0x80ccc580 !camellia_f	%f50,%f0,%f2,%f0
	.word	0x84cd4182 !camellia_f	%f52,%f2,%f0,%f2
	.word	0x80cdc580 !camellia_f	%f54,%f0,%f2,%f0
	.word	0x84ce4182 !camellia_f	%f56,%f2,%f0,%f2
	.word	0x80cec580 !camellia_f	%f58,%f0,%f2,%f0
	.word	0x81b76780 !camellia_fl	%f60,%f0,%f0
	.word	0x85b7e7a2 !camellia_fli	%f62,%f2,%f2
	.word	0x84cc0182 !camellia_f	%f16,%f2,%f0,%f2
	.word	0x80cc8580 !camellia_f	%f18,%f0,%f2,%f0
	ldd		[%i3 + 16], %f16
	ldd		[%i3 + 24], %f18
	.word	0x84cd0182 !camellia_f	%f20,%f2,%f0,%f2
	.word	0x80cd8580 !camellia_f	%f22,%f0,%f2,%f0
	ldd		[%i3 + 32], %f20
	ldd		[%i3 + 40], %f22
	.word	0x88ce0182 !camellia_f	%f24,%f2,%f0,%f4
	.word	0x84ce8980 !camellia_f	%f26,%f0,%f4,%f2
	ldd		[%i3 + 48], %f24
	ldd		[%i3 + 56], %f26
	.word	0x81b70d84 !fxor	%f28,%f4,%f0
	.word	0x85b78d82 !fxor	%f30,%f2,%f2
	ldd		[%i3 + 64], %f28
	retl
	ldd		[%i3 + 72], %f30
.type	_cmll256_encrypt_1x,#function
.size	_cmll256_encrypt_1x,.-_cmll256_encrypt_1x

.align	32
_cmll256_encrypt_2x:
	.word	0x84cc0182 !camellia_f	%f16,%f2,%f0,%f2
	.word	0x8ccc0986 !camellia_f	%f16,%f6,%f4,%f6
	.word	0x80cc8580 !camellia_f	%f18,%f0,%f2,%f0
	.word	0x88cc8d84 !camellia_f	%f18,%f4,%f6,%f4
	ldd		[%i3 + 208], %f16
	ldd		[%i3 + 216], %f18
	.word	0x84cd0182 !camellia_f	%f20,%f2,%f0,%f2
	.word	0x8ccd0986 !camellia_f	%f20,%f6,%f4,%f6
	.word	0x80cd8580 !camellia_f	%f22,%f0,%f2,%f0
	.word	0x88cd8d84 !camellia_f	%f22,%f4,%f6,%f4
	ldd		[%i3 + 224], %f20
	ldd		[%i3 + 232], %f22
	.word	0x84ce0182 !camellia_f	%f24,%f2,%f0,%f2
	.word	0x8cce0986 !camellia_f	%f24,%f6,%f4,%f6
	.word	0x80ce8580 !camellia_f	%f26,%f0,%f2,%f0
	.word	0x88ce8d84 !camellia_f	%f26,%f4,%f6,%f4
	ldd		[%i3 + 240], %f24
	ldd		[%i3 + 248], %f26
	.word	0x81b72780 !camellia_fl	%f28,%f0,%f0
	.word	0x89b72784 !camellia_fl	%f28,%f4,%f4
	.word	0x85b7a7a2 !camellia_fli	%f30,%f2,%f2
	.word	0x8db7a7a6 !camellia_fli	%f30,%f6,%f6
	ldd		[%i3 + 256], %f28
	ldd		[%i3 + 264], %f30
	.word	0x84c84182 !camellia_f	%f32,%f2,%f0,%f2
	.word	0x8cc84986 !camellia_f	%f32,%f6,%f4,%f6
	.word	0x80c8c580 !camellia_f	%f34,%f0,%f2,%f0
	.word	0x88c8cd84 !camellia_f	%f34,%f4,%f6,%f4
	.word	0x84c94182 !camellia_f	%f36,%f2,%f0,%f2
	.word	0x8cc94986 !camellia_f	%f36,%f6,%f4,%f6
	.word	0x80c9c580 !camellia_f	%f38,%f0,%f2,%f0
	.word	0x88c9cd84 !camellia_f	%f38,%f4,%f6,%f4
	.word	0x84ca4182 !camellia_f	%f40,%f2,%f0,%f2
	.word	0x8cca4986 !camellia_f	%f40,%f6,%f4,%f6
	.word	0x80cac580 !camellia_f	%f42,%f0,%f2,%f0
	.word	0x88cacd84 !camellia_f	%f42,%f4,%f6,%f4
	.word	0x81b36780 !camellia_fl	%f44,%f0,%f0
	.word	0x89b36784 !camellia_fl	%f44,%f4,%f4
	.word	0x85b3e7a2 !camellia_fli	%f46,%f2,%f2
	.word	0x8db3e7a6 !camellia_fli	%f46,%f6,%f6
	.word	0x84cc4182 !camellia_f	%f48,%f2,%f0,%f2
	.word	0x8ccc4986 !camellia_f	%f48,%f6,%f4,%f6
	.word	0x80ccc580 !camellia_f	%f50,%f0,%f2,%f0
	.word	0x88cccd84 !camellia_f	%f50,%f4,%f6,%f4
	.word	0x84cd4182 !camellia_f	%f52,%f2,%f0,%f2
	.word	0x8ccd4986 !camellia_f	%f52,%f6,%f4,%f6
	.word	0x80cdc580 !camellia_f	%f54,%f0,%f2,%f0
	.word	0x88cdcd84 !camellia_f	%f54,%f4,%f6,%f4
	.word	0x84ce4182 !camellia_f	%f56,%f2,%f0,%f2
	.word	0x8cce4986 !camellia_f	%f56,%f6,%f4,%f6
	.word	0x80cec580 !camellia_f	%f58,%f0,%f2,%f0
	.word	0x88cecd84 !camellia_f	%f58,%f4,%f6,%f4
	.word	0x81b76780 !camellia_fl	%f60,%f0,%f0
	.word	0x89b76784 !camellia_fl	%f60,%f4,%f4
	.word	0x85b7e7a2 !camellia_fli	%f62,%f2,%f2
	.word	0x8db7e7a6 !camellia_fli	%f62,%f6,%f6
	.word	0x84cc0182 !camellia_f	%f16,%f2,%f0,%f2
	.word	0x8ccc0986 !camellia_f	%f16,%f6,%f4,%f6
	.word	0x80cc8580 !camellia_f	%f18,%f0,%f2,%f0
	.word	0x88cc8d84 !camellia_f	%f18,%f4,%f6,%f4
	ldd		[%i3 + 16], %f16
	ldd		[%i3 + 24], %f18
	.word	0x84cd0182 !camellia_f	%f20,%f2,%f0,%f2
	.word	0x8ccd0986 !camellia_f	%f20,%f6,%f4,%f6
	.word	0x80cd8580 !camellia_f	%f22,%f0,%f2,%f0
	.word	0x88cd8d84 !camellia_f	%f22,%f4,%f6,%f4
	ldd		[%i3 + 32], %f20
	ldd		[%i3 + 40], %f22
	.word	0x90ce0182 !camellia_f	%f24,%f2,%f0,%f8
	.word	0x94ce0986 !camellia_f	%f24,%f6,%f4,%f10
	.word	0x84ce9180 !camellia_f	%f26,%f0,%f8,%f2
	.word	0x8cce9584 !camellia_f	%f26,%f4,%f10,%f6
	ldd		[%i3 + 48], %f24
	ldd		[%i3 + 56], %f26
	.word	0x81b70d88 !fxor	%f28,%f8,%f0
	.word	0x89b70d8a !fxor	%f28,%f10,%f4
	.word	0x85b78d82 !fxor	%f30,%f2,%f2
	.word	0x8db78d86 !fxor	%f30,%f6,%f6
	ldd		[%i3 + 64], %f28
	retl
	ldd		[%i3 + 72], %f30
.type	_cmll256_encrypt_2x,#function
.size	_cmll256_encrypt_2x,.-_cmll256_encrypt_2x

.align	32
_cmll256_decrypt_1x:
	.word	0x84cc0182 !camellia_f	%f16,%f2,%f0,%f2
	.word	0x80cc8580 !camellia_f	%f18,%f0,%f2,%f0
	ldd		[%i3 - 8], %f16
	ldd		[%i3 - 16], %f18
	.word	0x84cd0182 !camellia_f	%f20,%f2,%f0,%f2
	.word	0x80cd8580 !camellia_f	%f22,%f0,%f2,%f0
	ldd		[%i3 - 24], %f20
	ldd		[%i3 - 32], %f22
	.word	0x84ce0182 !camellia_f	%f24,%f2,%f0,%f2
	.word	0x80ce8580 !camellia_f	%f26,%f0,%f2,%f0
	ldd		[%i3 - 40], %f24
	ldd		[%i3 - 48], %f26
	.word	0x81b72780 !camellia_fl	%f28,%f0,%f0
	.word	0x85b7a7a2 !camellia_fli	%f30,%f2,%f2
	ldd		[%i3 - 56], %f28
	ldd		[%i3 - 64], %f30
	.word	0x84c84182 !camellia_f	%f32,%f2,%f0,%f2
	.word	0x80c8c580 !camellia_f	%f34,%f0,%f2,%f0
	.word	0x84c94182 !camellia_f	%f36,%f2,%f0,%f2
	.word	0x80c9c580 !camellia_f	%f38,%f0,%f2,%f0
	.word	0x84ca4182 !camellia_f	%f40,%f2,%f0,%f2
	.word	0x80cac580 !camellia_f	%f42,%f0,%f2,%f0
	.word	0x81b36780 !camellia_fl	%f44,%f0,%f0
	.word	0x85b3e7a2 !camellia_fli	%f46,%f2,%f2
	.word	0x84cc4182 !camellia_f	%f48,%f2,%f0,%f2
	.word	0x80ccc580 !camellia_f	%f50,%f0,%f2,%f0
	.word	0x84cd4182 !camellia_f	%f52,%f2,%f0,%f2
	.word	0x80cdc580 !camellia_f	%f54,%f0,%f2,%f0
	.word	0x84ce4182 !camellia_f	%f56,%f2,%f0,%f2
	.word	0x80cec580 !camellia_f	%f58,%f0,%f2,%f0
	.word	0x81b76780 !camellia_fl	%f60,%f0,%f0
	.word	0x85b7e7a2 !camellia_fli	%f62,%f2,%f2
	.word	0x84cc0182 !camellia_f	%f16,%f2,%f0,%f2
	.word	0x80cc8580 !camellia_f	%f18,%f0,%f2,%f0
	ldd		[%i3 + 184], %f16
	ldd		[%i3 + 176], %f18
	.word	0x84cd0182 !camellia_f	%f20,%f2,%f0,%f2
	.word	0x80cd8580 !camellia_f	%f22,%f0,%f2,%f0
	ldd		[%i3 + 168], %f20
	ldd		[%i3 + 160], %f22
	.word	0x88ce0182 !camellia_f	%f24,%f2,%f0,%f4
	.word	0x84ce8980 !camellia_f	%f26,%f0,%f4,%f2
	ldd		[%i3 + 152], %f24
	ldd		[%i3 + 144], %f26
	.word	0x81b78d84 !fxor	%f30,%f4,%f0
	.word	0x85b70d82 !fxor	%f28,%f2,%f2
	ldd		[%i3 + 136], %f28
	retl
	ldd		[%i3 + 128], %f30
.type	_cmll256_decrypt_1x,#function
.size	_cmll256_decrypt_1x,.-_cmll256_decrypt_1x

.align	32
_cmll256_decrypt_2x:
	.word	0x84cc0182 !camellia_f	%f16,%f2,%f0,%f2
	.word	0x8ccc0986 !camellia_f	%f16,%f6,%f4,%f6
	.word	0x80cc8580 !camellia_f	%f18,%f0,%f2,%f0
	.word	0x88cc8d84 !camellia_f	%f18,%f4,%f6,%f4
	ldd		[%i3 - 8], %f16
	ldd		[%i3 - 16], %f18
	.word	0x84cd0182 !camellia_f	%f20,%f2,%f0,%f2
	.word	0x8ccd0986 !camellia_f	%f20,%f6,%f4,%f6
	.word	0x80cd8580 !camellia_f	%f22,%f0,%f2,%f0
	.word	0x88cd8d84 !camellia_f	%f22,%f4,%f6,%f4
	ldd		[%i3 - 24], %f20
	ldd		[%i3 - 32], %f22
	.word	0x84ce0182 !camellia_f	%f24,%f2,%f0,%f2
	.word	0x8cce0986 !camellia_f	%f24,%f6,%f4,%f6
	.word	0x80ce8580 !camellia_f	%f26,%f0,%f2,%f0
	.word	0x88ce8d84 !camellia_f	%f26,%f4,%f6,%f4
	ldd		[%i3 - 40], %f24
	ldd		[%i3 - 48], %f26
	.word	0x81b72780 !camellia_fl	%f28,%f0,%f0
	.word	0x89b72784 !camellia_fl	%f28,%f4,%f4
	.word	0x85b7a7a2 !camellia_fli	%f30,%f2,%f2
	.word	0x8db7a7a6 !camellia_fli	%f30,%f6,%f6
	ldd		[%i3 - 56], %f28
	ldd		[%i3 - 64], %f30
	.word	0x84c84182 !camellia_f	%f32,%f2,%f0,%f2
	.word	0x8cc84986 !camellia_f	%f32,%f6,%f4,%f6
	.word	0x80c8c580 !camellia_f	%f34,%f0,%f2,%f0
	.word	0x88c8cd84 !camellia_f	%f34,%f4,%f6,%f4
	.word	0x84c94182 !camellia_f	%f36,%f2,%f0,%f2
	.word	0x8cc94986 !camellia_f	%f36,%f6,%f4,%f6
	.word	0x80c9c580 !camellia_f	%f38,%f0,%f2,%f0
	.word	0x88c9cd84 !camellia_f	%f38,%f4,%f6,%f4
	.word	0x84ca4182 !camellia_f	%f40,%f2,%f0,%f2
	.word	0x8cca4986 !camellia_f	%f40,%f6,%f4,%f6
	.word	0x80cac580 !camellia_f	%f42,%f0,%f2,%f0
	.word	0x88cacd84 !camellia_f	%f42,%f4,%f6,%f4
	.word	0x81b36780 !camellia_fl	%f44,%f0,%f0
	.word	0x89b36784 !camellia_fl	%f44,%f4,%f4
	.word	0x85b3e7a2 !camellia_fli	%f46,%f2,%f2
	.word	0x8db3e7a6 !camellia_fli	%f46,%f6,%f6
	.word	0x84cc4182 !camellia_f	%f48,%f2,%f0,%f2
	.word	0x8ccc4986 !camellia_f	%f48,%f6,%f4,%f6
	.word	0x80ccc580 !camellia_f	%f50,%f0,%f2,%f0
	.word	0x88cccd84 !camellia_f	%f50,%f4,%f6,%f4
	.word	0x84cd4182 !camellia_f	%f52,%f2,%f0,%f2
	.word	0x8ccd4986 !camellia_f	%f52,%f6,%f4,%f6
	.word	0x80cdc580 !camellia_f	%f54,%f0,%f2,%f0
	.word	0x88cdcd84 !camellia_f	%f54,%f4,%f6,%f4
	.word	0x84ce4182 !camellia_f	%f56,%f2,%f0,%f2
	.word	0x8cce4986 !camellia_f	%f56,%f6,%f4,%f6
	.word	0x80cec580 !camellia_f	%f58,%f0,%f2,%f0
	.word	0x88cecd84 !camellia_f	%f58,%f4,%f6,%f4
	.word	0x81b76780 !camellia_fl	%f60,%f0,%f0
	.word	0x89b76784 !camellia_fl	%f60,%f4,%f4
	.word	0x85b7e7a2 !camellia_fli	%f62,%f2,%f2
	.word	0x8db7e7a6 !camellia_fli	%f62,%f6,%f6
	.word	0x84cc0182 !camellia_f	%f16,%f2,%f0,%f2
	.word	0x8ccc0986 !camellia_f	%f16,%f6,%f4,%f6
	.word	0x80cc8580 !camellia_f	%f18,%f0,%f2,%f0
	.word	0x88cc8d84 !camellia_f	%f18,%f4,%f6,%f4
	ldd		[%i3 + 184], %f16
	ldd		[%i3 + 176], %f18
	.word	0x84cd0182 !camellia_f	%f20,%f2,%f0,%f2
	.word	0x8ccd0986 !camellia_f	%f20,%f6,%f4,%f6
	.word	0x80cd8580 !camellia_f	%f22,%f0,%f2,%f0
	.word	0x88cd8d84 !camellia_f	%f22,%f4,%f6,%f4
	ldd		[%i3 + 168], %f20
	ldd		[%i3 + 160], %f22
	.word	0x90ce0182 !camellia_f	%f24,%f2,%f0,%f8
	.word	0x94ce0986 !camellia_f	%f24,%f6,%f4,%f10
	.word	0x84ce9180 !camellia_f	%f26,%f0,%f8,%f2
	.word	0x8cce9584 !camellia_f	%f26,%f4,%f10,%f6
	ldd		[%i3 + 152], %f24
	ldd		[%i3 + 144], %f26
	.word	0x81b78d88 !fxor	%f30,%f8,%f0
	.word	0x89b78d8a !fxor	%f30,%f10,%f4
	.word	0x85b70d82 !fxor	%f28,%f2,%f2
	.word	0x8db70d86 !fxor	%f28,%f6,%f6
	ldd		[%i3 + 136], %f28
	retl
	ldd		[%i3 + 128], %f30
.type	_cmll256_decrypt_2x,#function
.size	_cmll256_decrypt_2x,.-_cmll256_decrypt_2x
.globl	cmll128_t4_cbc_encrypt
.align	32
cmll128_t4_cbc_encrypt:
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
	call		_cmll128_load_enckey
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
	call		_cmll128_encrypt_1x
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
	call		_cmll128_encrypt_1x
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
.type	cmll128_t4_cbc_encrypt,#function
.size	cmll128_t4_cbc_encrypt,.-cmll128_t4_cbc_encrypt
.globl	cmll256_t4_cbc_encrypt
.align	32
cmll256_t4_cbc_encrypt:
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
	call		_cmll256_load_enckey
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
	call		_cmll256_encrypt_1x
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
	call		_cmll256_encrypt_1x
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
.type	cmll256_t4_cbc_encrypt,#function
.size	cmll256_t4_cbc_encrypt,.-cmll256_t4_cbc_encrypt
.globl	cmll128_t4_cbc_decrypt
.align	32
cmll128_t4_cbc_decrypt:
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
	call		_cmll128_load_deckey
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
	call		_cmll128_decrypt_1x
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
	call		_cmll128_decrypt_2x
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
	call		_cmll128_decrypt_2x
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
.type	cmll128_t4_cbc_decrypt,#function
.size	cmll128_t4_cbc_decrypt,.-cmll128_t4_cbc_decrypt
.globl	cmll256_t4_cbc_decrypt
.align	32
cmll256_t4_cbc_decrypt:
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
	call		_cmll256_load_deckey
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
	call		_cmll256_decrypt_1x
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
	call		_cmll256_decrypt_2x
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
	call		_cmll256_decrypt_2x
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
.type	cmll256_t4_cbc_decrypt,#function
.size	cmll256_t4_cbc_decrypt,.-cmll256_t4_cbc_decrypt
.globl	cmll128_t4_ctr32_encrypt
.align	32
cmll128_t4_ctr32_encrypt:
	save		%sp, -192, %sp
	srln		%i2, 0, %i2		! needed on v8+, "nop" on v9

	prefetch	[%i0], 20
	prefetch	[%i0 + 63], 20
	call		_cmll128_load_enckey
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
	.word	0x84cc1d82 !camellia_f	%f16,%f2,%f14,%f2
	.word	0x80cc858e !camellia_f	%f18,%f14,%f2,%f0
	call		_cmll128_encrypt_1x+8
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
	.word	0x84cc1d82 !camellia_f	%f16,%f2,%f14,%f2
	.word	0x8ccc1d86 !camellia_f	%f16,%f6,%f14,%f6
	.word	0x80cc858e !camellia_f	%f18,%f14,%f2,%f0
	.word	0x88cc8d8e !camellia_f	%f18,%f14,%f6,%f4
	call		_cmll128_encrypt_2x+16
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
	.word	0x84cc1d82 !camellia_f	%f16,%f2,%f14,%f2
	.word	0x8ccc1d86 !camellia_f	%f16,%f6,%f14,%f6
	.word	0x80cc858e !camellia_f	%f18,%f14,%f2,%f0
	.word	0x88cc8d8e !camellia_f	%f18,%f14,%f6,%f4
	call		_cmll128_encrypt_2x+16
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
.type	cmll128_t4_ctr32_encrypt,#function
.size	cmll128_t4_ctr32_encrypt,.-cmll128_t4_ctr32_encrypt
.globl	cmll256_t4_ctr32_encrypt
.align	32
cmll256_t4_ctr32_encrypt:
	save		%sp, -192, %sp
	srln		%i2, 0, %i2		! needed on v8+, "nop" on v9

	prefetch	[%i0], 20
	prefetch	[%i0 + 63], 20
	call		_cmll256_load_enckey
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
	.word	0x84cc1d82 !camellia_f	%f16,%f2,%f14,%f2
	.word	0x80cc858e !camellia_f	%f18,%f14,%f2,%f0
	call		_cmll256_encrypt_1x+8
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
	.word	0x84cc1d82 !camellia_f	%f16,%f2,%f14,%f2
	.word	0x8ccc1d86 !camellia_f	%f16,%f6,%f14,%f6
	.word	0x80cc858e !camellia_f	%f18,%f14,%f2,%f0
	.word	0x88cc8d8e !camellia_f	%f18,%f14,%f6,%f4
	call		_cmll256_encrypt_2x+16
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
	.word	0x84cc1d82 !camellia_f	%f16,%f2,%f14,%f2
	.word	0x8ccc1d86 !camellia_f	%f16,%f6,%f14,%f6
	.word	0x80cc858e !camellia_f	%f18,%f14,%f2,%f0
	.word	0x88cc8d8e !camellia_f	%f18,%f14,%f6,%f4
	call		_cmll256_encrypt_2x+16
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
.type	cmll256_t4_ctr32_encrypt,#function
.size	cmll256_t4_ctr32_encrypt,.-cmll256_t4_ctr32_encrypt
