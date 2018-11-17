.register       %g2,#scratch
.register       %g3,#scratch
.text
.align	32
.globl	des_t4_key_expand
.type	des_t4_key_expand,#function
des_t4_key_expand:
	andcc		%o0, 0x7, %g0
	.word	0x91b20300 !alignaddr	%o0,%g0,%o0
	bz,pt		%icc, 1f
	ldd		[%o0 + 0x00], %f0
	ldd		[%o0 + 0x08], %f2
	.word	0x81b00902 !faligndata	%f0,%f2,%f0
1:	.word	0x81b026c0 !des_kexpand	%f0,0,%f0,
	.word	0x85b026c1 !des_kexpand	%f0,1,%f2,
	std		%f0, [%o1 + 0x00]
	.word	0x8db0a6c3 !des_kexpand	%f2,3,%f6,
	std		%f2, [%o1 + 0x08]
	.word	0x89b0a6c2 !des_kexpand	%f2,2,%f4,
	.word	0x95b1a6c3 !des_kexpand	%f6,3,%f10,
	std		%f6, [%o1 + 0x18]
	.word	0x91b1a6c2 !des_kexpand	%f6,2,%f8,
	std		%f4, [%o1 + 0x10]
	.word	0x9db2a6c3 !des_kexpand	%f10,3,%f14,
	std		%f10, [%o1 + 0x28]
	.word	0x99b2a6c2 !des_kexpand	%f10,2,%f12,
	std		%f8, [%o1 + 0x20]
	.word	0xa1b3a6c1 !des_kexpand	%f14,1,%f16,
	std		%f14, [%o1 + 0x38]
	.word	0xa9b426c3 !des_kexpand	%f16,3,%f20,
	std		%f12, [%o1 + 0x30]
	.word	0xa5b426c2 !des_kexpand	%f16,2,%f18,
	std		%f16, [%o1 + 0x40]
	.word	0xb1b526c3 !des_kexpand	%f20,3,%f24,
	std		%f20, [%o1 + 0x50]
	.word	0xadb526c2 !des_kexpand	%f20,2,%f22,
	std		%f18, [%o1 + 0x48]
	.word	0xb9b626c3 !des_kexpand	%f24,3,%f28,
	std		%f24, [%o1 + 0x60]
	.word	0xb5b626c2 !des_kexpand	%f24,2,%f26,
	std		%f22, [%o1 + 0x58]
	.word	0xbdb726c1 !des_kexpand	%f28,1,%f30,
	std		%f28, [%o1 + 0x70]
	std		%f26, [%o1 + 0x68]
	retl
	std		%f30, [%o1 + 0x78]
.size	des_t4_key_expand,.-des_t4_key_expand
.globl	des_t4_cbc_encrypt
.align	32
des_t4_cbc_encrypt:
	cmp		%o2, 0
	be,pn		%xcc, .Lcbc_abort
	srln		%o2, 0, %o2		! needed on v8+, "nop" on v9
	ld		[%o4 + 0], %f0	! load ivec
	ld		[%o4 + 4], %f1

	and		%o0, 7, %g1
	andn		%o0, 7, %o0
	sll		%g1, 3, %g1
	mov		0xff, %g3
	prefetch	[%o0], 20
	prefetch	[%o0 + 63], 20
	sub		%g0, %g1, %g2
	and		%o1, 7, %g4
	.word	0x93b24340 !alignaddrl	%o1,%g0,%o1
	srl		%g3, %g4, %g3
	srlx		%o2, 3, %o2
	movrz		%g4, 0, %g3
	prefetch	[%o1], 22

	ldd		[%o3 + 0x00], %f4	! load key schedule
	ldd		[%o3 + 0x08], %f6
	ldd		[%o3 + 0x10], %f8
	ldd		[%o3 + 0x18], %f10
	ldd		[%o3 + 0x20], %f12
	ldd		[%o3 + 0x28], %f14
	ldd		[%o3 + 0x30], %f16
	ldd		[%o3 + 0x38], %f18
	ldd		[%o3 + 0x40], %f20
	ldd		[%o3 + 0x48], %f22
	ldd		[%o3 + 0x50], %f24
	ldd		[%o3 + 0x58], %f26
	ldd		[%o3 + 0x60], %f28
	ldd		[%o3 + 0x68], %f30
	ldd		[%o3 + 0x70], %f32
	ldd		[%o3 + 0x78], %f34

.Ldes_cbc_enc_loop:
	ldx		[%o0 + 0], %g4
	brz,pt		%g1, 4f
	nop

	ldx		[%o0 + 8], %g5
	sllx		%g4, %g1, %g4
	srlx		%g5, %g2, %g5
	or		%g5, %g4, %g4
4:
	.word	0x85b02304 !movxtod	%g4,%f2
	prefetch	[%o0 + 8+63], 20
	add		%o0, 8, %o0
	.word	0x81b08d80 !fxor	%f2,%f0,%f0		! ^= ivec
	prefetch	[%o1 + 63], 22

	.word	0x81b02680 !des_ip	%f0,%f0,,
	.word	0x80c90126 !des_round	%f4,%f6,%f0,%f0
	.word	0x80ca012a !des_round	%f8,%f10,%f0,%f0
	.word	0x80cb012e !des_round	%f12,%f14,%f0,%f0
	.word	0x80cc0132 !des_round	%f16,%f18,%f0,%f0
	.word	0x80cd0136 !des_round	%f20,%f22,%f0,%f0
	.word	0x80ce013a !des_round	%f24,%f26,%f0,%f0
	.word	0x80cf013e !des_round	%f28,%f30,%f0,%f0
	.word	0x80c84123 !des_round	%f32,%f34,%f0,%f0
	.word	0x81b026a0 !des_iip	%f0,%f0,,

	brnz,pn		%g3, 2f
	sub		%o2, 1, %o2

	std		%f0, [%o1 + 0]
	brnz,pt		%o2, .Ldes_cbc_enc_loop
	add		%o1, 8, %o1

	st		%f0, [%o4 + 0]	! write out ivec
	retl
	st		%f1, [%o4 + 4]
.Lcbc_abort:
	retl
	nop

.align	16
2:	ldxa		[%o0]0x82, %g4		! avoid read-after-write hazard
						! and ~4x deterioration
						! in inp==out case
	.word	0x85b00900 !faligndata	%f0,%f0,%f2		! handle unaligned output

	stda		%f2, [%o1 + %g3]0xc0	! partial store
	add		%o1, 8, %o1
	orn		%g0, %g3, %g3
	stda		%f2, [%o1 + %g3]0xc0	! partial store

	brnz,pt		%o2, .Ldes_cbc_enc_loop+4
	orn		%g0, %g3, %g3

	st		%f0, [%o4 + 0]	! write out ivec
	retl
	st		%f1, [%o4 + 4]
.type	des_t4_cbc_encrypt,#function
.size	des_t4_cbc_encrypt,.-des_t4_cbc_encrypt

.globl	des_t4_cbc_decrypt
.align	32
des_t4_cbc_decrypt:
	cmp		%o2, 0
	be,pn		%xcc, .Lcbc_abort
	srln		%o2, 0, %o2		! needed on v8+, "nop" on v9
	ld		[%o4 + 0], %f2	! load ivec
	ld		[%o4 + 4], %f3

	and		%o0, 7, %g1
	andn		%o0, 7, %o0
	sll		%g1, 3, %g1
	mov		0xff, %g3
	prefetch	[%o0], 20
	prefetch	[%o0 + 63], 20
	sub		%g0, %g1, %g2
	and		%o1, 7, %g4
	.word	0x93b24340 !alignaddrl	%o1,%g0,%o1
	srl		%g3, %g4, %g3
	srlx		%o2, 3, %o2
	movrz		%g4, 0, %g3
	prefetch	[%o1], 22

	ldd		[%o3 + 0x78], %f4	! load key schedule
	ldd		[%o3 + 0x70], %f6
	ldd		[%o3 + 0x68], %f8
	ldd		[%o3 + 0x60], %f10
	ldd		[%o3 + 0x58], %f12
	ldd		[%o3 + 0x50], %f14
	ldd		[%o3 + 0x48], %f16
	ldd		[%o3 + 0x40], %f18
	ldd		[%o3 + 0x38], %f20
	ldd		[%o3 + 0x30], %f22
	ldd		[%o3 + 0x28], %f24
	ldd		[%o3 + 0x20], %f26
	ldd		[%o3 + 0x18], %f28
	ldd		[%o3 + 0x10], %f30
	ldd		[%o3 + 0x08], %f32
	ldd		[%o3 + 0x00], %f34

.Ldes_cbc_dec_loop:
	ldx		[%o0 + 0], %g4
	brz,pt		%g1, 4f
	nop

	ldx		[%o0 + 8], %g5
	sllx		%g4, %g1, %g4
	srlx		%g5, %g2, %g5
	or		%g5, %g4, %g4
4:
	.word	0x81b02304 !movxtod	%g4,%f0
	prefetch	[%o0 + 8+63], 20
	add		%o0, 8, %o0
	prefetch	[%o1 + 63], 22

	.word	0x81b02680 !des_ip	%f0,%f0,,
	.word	0x80c90126 !des_round	%f4,%f6,%f0,%f0
	.word	0x80ca012a !des_round	%f8,%f10,%f0,%f0
	.word	0x80cb012e !des_round	%f12,%f14,%f0,%f0
	.word	0x80cc0132 !des_round	%f16,%f18,%f0,%f0
	.word	0x80cd0136 !des_round	%f20,%f22,%f0,%f0
	.word	0x80ce013a !des_round	%f24,%f26,%f0,%f0
	.word	0x80cf013e !des_round	%f28,%f30,%f0,%f0
	.word	0x80c84123 !des_round	%f32,%f34,%f0,%f0
	.word	0x81b026a0 !des_iip	%f0,%f0,,

	.word	0x81b08d80 !fxor	%f2,%f0,%f0		! ^= ivec
	.word	0x85b02304 !movxtod	%g4,%f2

	brnz,pn		%g3, 2f
	sub		%o2, 1, %o2

	std		%f0, [%o1 + 0]
	brnz,pt		%o2, .Ldes_cbc_dec_loop
	add		%o1, 8, %o1

	st		%f2, [%o4 + 0]	! write out ivec
	retl
	st		%f3, [%o4 + 4]

.align	16
2:	ldxa		[%o0]0x82, %g4		! avoid read-after-write hazard
						! and ~4x deterioration
						! in inp==out case
	.word	0x81b00900 !faligndata	%f0,%f0,%f0		! handle unaligned output

	stda		%f0, [%o1 + %g3]0xc0	! partial store
	add		%o1, 8, %o1
	orn		%g0, %g3, %g3
	stda		%f0, [%o1 + %g3]0xc0	! partial store

	brnz,pt		%o2, .Ldes_cbc_dec_loop+4
	orn		%g0, %g3, %g3

	st		%f2, [%o4 + 0]	! write out ivec
	retl
	st		%f3, [%o4 + 4]
.type	des_t4_cbc_decrypt,#function
.size	des_t4_cbc_decrypt,.-des_t4_cbc_decrypt
.globl	des_t4_ede3_cbc_encrypt
.align	32
des_t4_ede3_cbc_encrypt:
	cmp		%o2, 0
	be,pn		%xcc, .Lcbc_abort
	srln		%o2, 0, %o2		! needed on v8+, "nop" on v9
	ld		[%o4 + 0], %f0	! load ivec
	ld		[%o4 + 4], %f1

	and		%o0, 7, %g1
	andn		%o0, 7, %o0
	sll		%g1, 3, %g1
	mov		0xff, %g3
	prefetch	[%o0], 20
	prefetch	[%o0 + 63], 20
	sub		%g0, %g1, %g2
	and		%o1, 7, %g4
	.word	0x93b24340 !alignaddrl	%o1,%g0,%o1
	srl		%g3, %g4, %g3
	srlx		%o2, 3, %o2
	movrz		%g4, 0, %g3
	prefetch	[%o1], 22

	ldd		[%o3 + 0x00], %f4	! load key schedule
	ldd		[%o3 + 0x08], %f6
	ldd		[%o3 + 0x10], %f8
	ldd		[%o3 + 0x18], %f10
	ldd		[%o3 + 0x20], %f12
	ldd		[%o3 + 0x28], %f14
	ldd		[%o3 + 0x30], %f16
	ldd		[%o3 + 0x38], %f18
	ldd		[%o3 + 0x40], %f20
	ldd		[%o3 + 0x48], %f22
	ldd		[%o3 + 0x50], %f24
	ldd		[%o3 + 0x58], %f26
	ldd		[%o3 + 0x60], %f28
	ldd		[%o3 + 0x68], %f30
	ldd		[%o3 + 0x70], %f32
	ldd		[%o3 + 0x78], %f34

.Ldes_ede3_cbc_enc_loop:
	ldx		[%o0 + 0], %g4
	brz,pt		%g1, 4f
	nop

	ldx		[%o0 + 8], %g5
	sllx		%g4, %g1, %g4
	srlx		%g5, %g2, %g5
	or		%g5, %g4, %g4
4:
	.word	0x85b02304 !movxtod	%g4,%f2
	prefetch	[%o0 + 8+63], 20
	add		%o0, 8, %o0
	.word	0x81b08d80 !fxor	%f2,%f0,%f0		! ^= ivec
	prefetch	[%o1 + 63], 22

	.word	0x81b02680 !des_ip	%f0,%f0,,
	.word	0x80c90126 !des_round	%f4,%f6,%f0,%f0
	.word	0x80ca012a !des_round	%f8,%f10,%f0,%f0
	.word	0x80cb012e !des_round	%f12,%f14,%f0,%f0
	.word	0x80cc0132 !des_round	%f16,%f18,%f0,%f0
	ldd		[%o3 + 0x100-0x08], %f36
	ldd		[%o3 + 0x100-0x10], %f38
	.word	0x80cd0136 !des_round	%f20,%f22,%f0,%f0
	ldd		[%o3 + 0x100-0x18], %f40
	ldd		[%o3 + 0x100-0x20], %f42
	.word	0x80ce013a !des_round	%f24,%f26,%f0,%f0
	ldd		[%o3 + 0x100-0x28], %f44
	ldd		[%o3 + 0x100-0x30], %f46
	.word	0x80cf013e !des_round	%f28,%f30,%f0,%f0
	ldd		[%o3 + 0x100-0x38], %f48
	ldd		[%o3 + 0x100-0x40], %f50
	.word	0x80c84123 !des_round	%f32,%f34,%f0,%f0
	ldd		[%o3 + 0x100-0x48], %f52
	ldd		[%o3 + 0x100-0x50], %f54
	.word	0x81b026a0 !des_iip	%f0,%f0,,

	ldd		[%o3 + 0x100-0x58], %f56
	ldd		[%o3 + 0x100-0x60], %f58
	.word	0x81b02680 !des_ip	%f0,%f0,,
	ldd		[%o3 + 0x100-0x68], %f60
	ldd		[%o3 + 0x100-0x70], %f62
	.word	0x80c94127 !des_round	%f36,%f38,%f0,%f0
	ldd		[%o3 + 0x100-0x78], %f36
	ldd		[%o3 + 0x100-0x80], %f38
	.word	0x80ca412b !des_round	%f40,%f42,%f0,%f0
	.word	0x80cb412f !des_round	%f44,%f46,%f0,%f0
	.word	0x80cc4133 !des_round	%f48,%f50,%f0,%f0
	ldd		[%o3 + 0x100+0x00], %f40
	ldd		[%o3 + 0x100+0x08], %f42
	.word	0x80cd4137 !des_round	%f52,%f54,%f0,%f0
	ldd		[%o3 + 0x100+0x10], %f44
	ldd		[%o3 + 0x100+0x18], %f46
	.word	0x80ce413b !des_round	%f56,%f58,%f0,%f0
	ldd		[%o3 + 0x100+0x20], %f48
	ldd		[%o3 + 0x100+0x28], %f50
	.word	0x80cf413f !des_round	%f60,%f62,%f0,%f0
	ldd		[%o3 + 0x100+0x30], %f52
	ldd		[%o3 + 0x100+0x38], %f54
	.word	0x80c94127 !des_round	%f36,%f38,%f0,%f0
	ldd		[%o3 + 0x100+0x40], %f56
	ldd		[%o3 + 0x100+0x48], %f58
	.word	0x81b026a0 !des_iip	%f0,%f0,,

	ldd		[%o3 + 0x100+0x50], %f60
	ldd		[%o3 + 0x100+0x58], %f62
	.word	0x81b02680 !des_ip	%f0,%f0,,
	ldd		[%o3 + 0x100+0x60], %f36
	ldd		[%o3 + 0x100+0x68], %f38
	.word	0x80ca412b !des_round	%f40,%f42,%f0,%f0
	ldd		[%o3 + 0x100+0x70], %f40
	ldd		[%o3 + 0x100+0x78], %f42
	.word	0x80cb412f !des_round	%f44,%f46,%f0,%f0
	.word	0x80cc4133 !des_round	%f48,%f50,%f0,%f0
	.word	0x80cd4137 !des_round	%f52,%f54,%f0,%f0
	.word	0x80ce413b !des_round	%f56,%f58,%f0,%f0
	.word	0x80cf413f !des_round	%f60,%f62,%f0,%f0
	.word	0x80c94127 !des_round	%f36,%f38,%f0,%f0
	.word	0x80ca412b !des_round	%f40,%f42,%f0,%f0
	.word	0x81b026a0 !des_iip	%f0,%f0,,

	brnz,pn		%g3, 2f
	sub		%o2, 1, %o2

	std		%f0, [%o1 + 0]
	brnz,pt		%o2, .Ldes_ede3_cbc_enc_loop
	add		%o1, 8, %o1

	st		%f0, [%o4 + 0]	! write out ivec
	retl
	st		%f1, [%o4 + 4]

.align	16
2:	ldxa		[%o0]0x82, %g4		! avoid read-after-write hazard
						! and ~2x deterioration
						! in inp==out case
	.word	0x85b00900 !faligndata	%f0,%f0,%f2		! handle unaligned output

	stda		%f2, [%o1 + %g3]0xc0	! partial store
	add		%o1, 8, %o1
	orn		%g0, %g3, %g3
	stda		%f2, [%o1 + %g3]0xc0	! partial store

	brnz,pt		%o2, .Ldes_ede3_cbc_enc_loop+4
	orn		%g0, %g3, %g3

	st		%f0, [%o4 + 0]	! write out ivec
	retl
	st		%f1, [%o4 + 4]
.type	des_t4_ede3_cbc_encrypt,#function
.size	des_t4_ede3_cbc_encrypt,.-des_t4_ede3_cbc_encrypt

.globl	des_t4_ede3_cbc_decrypt
.align	32
des_t4_ede3_cbc_decrypt:
	cmp		%o2, 0
	be,pn		%xcc, .Lcbc_abort
	srln		%o2, 0, %o2		! needed on v8+, "nop" on v9
	ld		[%o4 + 0], %f2	! load ivec
	ld		[%o4 + 4], %f3

	and		%o0, 7, %g1
	andn		%o0, 7, %o0
	sll		%g1, 3, %g1
	mov		0xff, %g3
	prefetch	[%o0], 20
	prefetch	[%o0 + 63], 20
	sub		%g0, %g1, %g2
	and		%o1, 7, %g4
	.word	0x93b24340 !alignaddrl	%o1,%g0,%o1
	srl		%g3, %g4, %g3
	srlx		%o2, 3, %o2
	movrz		%g4, 0, %g3
	prefetch	[%o1], 22

	ldd		[%o3 + 0x100+0x78], %f4	! load key schedule
	ldd		[%o3 + 0x100+0x70], %f6
	ldd		[%o3 + 0x100+0x68], %f8
	ldd		[%o3 + 0x100+0x60], %f10
	ldd		[%o3 + 0x100+0x58], %f12
	ldd		[%o3 + 0x100+0x50], %f14
	ldd		[%o3 + 0x100+0x48], %f16
	ldd		[%o3 + 0x100+0x40], %f18
	ldd		[%o3 + 0x100+0x38], %f20
	ldd		[%o3 + 0x100+0x30], %f22
	ldd		[%o3 + 0x100+0x28], %f24
	ldd		[%o3 + 0x100+0x20], %f26
	ldd		[%o3 + 0x100+0x18], %f28
	ldd		[%o3 + 0x100+0x10], %f30
	ldd		[%o3 + 0x100+0x08], %f32
	ldd		[%o3 + 0x100+0x00], %f34

.Ldes_ede3_cbc_dec_loop:
	ldx		[%o0 + 0], %g4
	brz,pt		%g1, 4f
	nop

	ldx		[%o0 + 8], %g5
	sllx		%g4, %g1, %g4
	srlx		%g5, %g2, %g5
	or		%g5, %g4, %g4
4:
	.word	0x81b02304 !movxtod	%g4,%f0
	prefetch	[%o0 + 8+63], 20
	add		%o0, 8, %o0
	prefetch	[%o1 + 63], 22

	.word	0x81b02680 !des_ip	%f0,%f0,,
	.word	0x80c90126 !des_round	%f4,%f6,%f0,%f0
	.word	0x80ca012a !des_round	%f8,%f10,%f0,%f0
	.word	0x80cb012e !des_round	%f12,%f14,%f0,%f0
	.word	0x80cc0132 !des_round	%f16,%f18,%f0,%f0
	ldd		[%o3 + 0x80+0x00], %f36
	ldd		[%o3 + 0x80+0x08], %f38
	.word	0x80cd0136 !des_round	%f20,%f22,%f0,%f0
	ldd		[%o3 + 0x80+0x10], %f40
	ldd		[%o3 + 0x80+0x18], %f42
	.word	0x80ce013a !des_round	%f24,%f26,%f0,%f0
	ldd		[%o3 + 0x80+0x20], %f44
	ldd		[%o3 + 0x80+0x28], %f46
	.word	0x80cf013e !des_round	%f28,%f30,%f0,%f0
	ldd		[%o3 + 0x80+0x30], %f48
	ldd		[%o3 + 0x80+0x38], %f50
	.word	0x80c84123 !des_round	%f32,%f34,%f0,%f0
	ldd		[%o3 + 0x80+0x40], %f52
	ldd		[%o3 + 0x80+0x48], %f54
	.word	0x81b026a0 !des_iip	%f0,%f0,,

	ldd		[%o3 + 0x80+0x50], %f56
	ldd		[%o3 + 0x80+0x58], %f58
	.word	0x81b02680 !des_ip	%f0,%f0,,
	ldd		[%o3 + 0x80+0x60], %f60
	ldd		[%o3 + 0x80+0x68], %f62
	.word	0x80c94127 !des_round	%f36,%f38,%f0,%f0
	ldd		[%o3 + 0x80+0x70], %f36
	ldd		[%o3 + 0x80+0x78], %f38
	.word	0x80ca412b !des_round	%f40,%f42,%f0,%f0
	.word	0x80cb412f !des_round	%f44,%f46,%f0,%f0
	.word	0x80cc4133 !des_round	%f48,%f50,%f0,%f0
	ldd		[%o3 + 0x80-0x08], %f40
	ldd		[%o3 + 0x80-0x10], %f42
	.word	0x80cd4137 !des_round	%f52,%f54,%f0,%f0
	ldd		[%o3 + 0x80-0x18], %f44
	ldd		[%o3 + 0x80-0x20], %f46
	.word	0x80ce413b !des_round	%f56,%f58,%f0,%f0
	ldd		[%o3 + 0x80-0x28], %f48
	ldd		[%o3 + 0x80-0x30], %f50
	.word	0x80cf413f !des_round	%f60,%f62,%f0,%f0
	ldd		[%o3 + 0x80-0x38], %f52
	ldd		[%o3 + 0x80-0x40], %f54
	.word	0x80c94127 !des_round	%f36,%f38,%f0,%f0
	ldd		[%o3 + 0x80-0x48], %f56
	ldd		[%o3 + 0x80-0x50], %f58
	.word	0x81b026a0 !des_iip	%f0,%f0,,

	ldd		[%o3 + 0x80-0x58], %f60
	ldd		[%o3 + 0x80-0x60], %f62
	.word	0x81b02680 !des_ip	%f0,%f0,,
	ldd		[%o3 + 0x80-0x68], %f36
	ldd		[%o3 + 0x80-0x70], %f38
	.word	0x80ca412b !des_round	%f40,%f42,%f0,%f0
	ldd		[%o3 + 0x80-0x78], %f40
	ldd		[%o3 + 0x80-0x80], %f42
	.word	0x80cb412f !des_round	%f44,%f46,%f0,%f0
	.word	0x80cc4133 !des_round	%f48,%f50,%f0,%f0
	.word	0x80cd4137 !des_round	%f52,%f54,%f0,%f0
	.word	0x80ce413b !des_round	%f56,%f58,%f0,%f0
	.word	0x80cf413f !des_round	%f60,%f62,%f0,%f0
	.word	0x80c94127 !des_round	%f36,%f38,%f0,%f0
	.word	0x80ca412b !des_round	%f40,%f42,%f0,%f0
	.word	0x81b026a0 !des_iip	%f0,%f0,,

	.word	0x81b08d80 !fxor	%f2,%f0,%f0		! ^= ivec
	.word	0x85b02304 !movxtod	%g4,%f2

	brnz,pn		%g3, 2f
	sub		%o2, 1, %o2

	std		%f0, [%o1 + 0]
	brnz,pt		%o2, .Ldes_ede3_cbc_dec_loop
	add		%o1, 8, %o1

	st		%f2, [%o4 + 0]	! write out ivec
	retl
	st		%f3, [%o4 + 4]

.align	16
2:	ldxa		[%o0]0x82, %g4		! avoid read-after-write hazard
						! and ~3x deterioration
						! in inp==out case
	.word	0x81b00900 !faligndata	%f0,%f0,%f0		! handle unaligned output

	stda		%f0, [%o1 + %g3]0xc0	! partial store
	add		%o1, 8, %o1
	orn		%g0, %g3, %g3
	stda		%f0, [%o1 + %g3]0xc0	! partial store

	brnz,pt		%o2, .Ldes_ede3_cbc_dec_loop+4
	orn		%g0, %g3, %g3

	st		%f2, [%o4 + 0]	! write out ivec
	retl
	st		%f3, [%o4 + 4]
.type	des_t4_ede3_cbc_decrypt,#function
.size	des_t4_ede3_cbc_decrypt,.-des_t4_ede3_cbc_decrypt
.asciz  "DES for SPARC T4, David S. Miller, Andy Polyakov"
.align  4
