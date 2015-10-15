.section	".text",#alloc,#execinstr

.align	64
vis_const:
.long	0x5a827999,0x5a827999	! K_00_19
.long	0x6ed9eba1,0x6ed9eba1	! K_20_39
.long	0x8f1bbcdc,0x8f1bbcdc	! K_40_59
.long	0xca62c1d6,0xca62c1d6	! K_60_79
.long	0x00000100,0x00000100
.align	64
.type	vis_const,#object
.size	vis_const,(.-vis_const)

.globl	sha1_block_data_order
sha1_block_data_order:
	save	%sp,-192,%sp
	add	%fp,2047-256,%g1

1:	call	.+8
	add	%o7,vis_const-1b,%i3

	ldd	[%i3+0],%f34
	ldd	[%i3+8],%f36
	ldd	[%i3+16],%f38
	ldd	[%i3+24],%f40
	ldd	[%i3+32],%f32

	ld	[%i0+0],%o0
	and	%g1,-256,%g1
	ld	[%i0+4],%o1
	sub	%g1,2047+192,%sp
	ld	[%i0+8],%o2
	and	%i1,7,%g4
	ld	[%i0+12],%o3
	and	%i1,-8,%i1
	ld	[%i0+16],%o4

	! X[16] is maintained in FP register bank
	.word	0x81b00304 !alignaddr	%g0,%g4,%g0
	ldd		[%i1+0],%f0
	sub		%i1,-64,%o5
	ldd		[%i1+8],%f2
	and		%o5,-64,%o5
	ldd		[%i1+16],%f4
	and		%o5,255,%o5
	ldd		[%i1+24],%f6
	add		%g1,%o5,%o5
	ldd		[%i1+32],%f8
	ldd		[%i1+40],%f10
	ldd		[%i1+48],%f12
	brz,pt		%g4,.Laligned
	ldd		[%i1+56],%f14

	ldd		[%i1+64],%f16
	.word	0x81b00902 !faligndata	%f0,%f2,%f0
	.word	0x85b08904 !faligndata	%f2,%f4,%f2
	.word	0x89b10906 !faligndata	%f4,%f6,%f4
	.word	0x8db18908 !faligndata	%f6,%f8,%f6
	.word	0x91b2090a !faligndata	%f8,%f10,%f8
	.word	0x95b2890c !faligndata	%f10,%f12,%f10
	.word	0x99b3090e !faligndata	%f12,%f14,%f12
	.word	0x9db38910 !faligndata	%f14,%f16,%f14

.Laligned:
	mov		5,%i3
	dec		1,%i2
	.word	0x81b0031b !alignaddr	%g0,%i3,%g0
	.word	0xa1b0ca40 !fpadd32	%f34,%f0,%f16
	.word	0xa5b0ca42 !fpadd32	%f34,%f2,%f18
	.word	0xa9b0ca44 !fpadd32	%f34,%f4,%f20
	.word	0xadb0ca46 !fpadd32	%f34,%f6,%f22
	.word	0xb1b0ca48 !fpadd32	%f34,%f8,%f24
	.word	0xb5b0ca4a !fpadd32	%f34,%f10,%f26
	.word	0xb9b0ca4c !fpadd32	%f34,%f12,%f28
	.word	0xbdb0ca4e !fpadd32	%f34,%f14,%f30
	std		%f16,[%o5+0]
	mov		%o0,%l0
	std		%f18,[%o5+8]
	mov		%o1,%l1
	std		%f20,[%o5+16]
	mov		%o2,%l2
	std		%f22,[%o5+24]
	mov		%o3,%l3
	std		%f24,[%o5+32]
	mov		%o4,%l4
	std		%f26,[%o5+40]
	.word	0x81b34da0 !fxors	%f13,%f0,%f0
	std		%f28,[%o5+48]
	ba		.Loop
	std		%f30,[%o5+56]
.align	32
.Loop:
	sll		%l0,5,%i3			!! 0
	and		%l2,%l1,%g5
	ld		[%o5+0],%o7
	 .word	0x83b38da1 !fxors	%f14,%f1,%f1! 0/ 0/ 0:X[1]^=X[14]
	srl		%l0,27,%i4
	add		%i3,%l4,%l4
	 .word	0xa5b08d88 !fxor	%f2,%f8,%f18! 1/ 1/ 1:Tmp=X[2,3]^X[8,9]
	sll		%l1,30,%i5
	add		%i4,%l4,%l4
	andn		%l3,%l1,%i4
	add		%o7,%l4,%l4
	 .word	0x81b48d80 !fxor	%f18,%f0,%f0		! 2/ 4/ 3:X[0,1]^=X[2,3]^X[8,9]
	srl		%l1,2,%l1
	or		%i4,%g5,%i4
	or		%i5,%l1,%l1
	add		%i4,%l4,%l4
	 .word	0xa5b00900 !faligndata	%f0,%f0,%f18		! 3/ 7/ 5:Tmp=X[0,1]>>>24
	sll		%l4,5,%i3			!! 1
	and		%l1,%l0,%g5
	ld		[%o5+4],%o7
	 .word	0x81b00a40 !fpadd32	%f0,%f0,%f0		! 4/ 8/ 6:X[0,1]<<=1
	srl		%l4,27,%i4
	add		%i3,%l3,%l3
	 .word	0xa5b486e1 !fmul8ulx16	%f18,%f32,%f18			! 5/10/ 7:Tmp>>=7, Tmp&=1
	sll		%l0,30,%i5
	add		%i4,%l3,%l3
	 .word	0xa9b0ca4e !fpadd32	%f34,%f14,%f20			!
	andn		%l2,%l0,%i4
	add		%o7,%l3,%l3
	 .word	0x85b3cda2 !fxors	%f15,%f2,%f2	!-1/-1/-1:X[0]^=X[13]
	srl		%l0,2,%l0
	or		%i4,%g5,%i4
	 .word	0x81b48d80 !fxor	%f18,%f0,%f0		! 8/14/10:X[0,1]|=Tmp
	or		%i5,%l0,%l0
	add		%i4,%l3,%l3
	sll		%l3,5,%i3			!! 2
	and		%l0,%l4,%g5
	ld		[%o5+8],%o7
	 .word	0x87b00da3 !fxors	%f0,%f3,%f3! 0/ 0/ 0:X[1]^=X[14]
	srl		%l3,27,%i4
	add		%i3,%l2,%l2
	 .word	0xa5b10d8a !fxor	%f4,%f10,%f18! 1/ 1/ 1:Tmp=X[2,3]^X[8,9]
	sll		%l4,30,%i5
	add		%i4,%l2,%l2
	andn		%l1,%l4,%i4
	add		%o7,%l2,%l2
	 .word	0x85b48d82 !fxor	%f18,%f2,%f2		! 2/ 4/ 3:X[0,1]^=X[2,3]^X[8,9]
	srl		%l4,2,%l4
	or		%i4,%g5,%i4
	or		%i5,%l4,%l4
	add		%i4,%l2,%l2
	 .word	0xa5b08902 !faligndata	%f2,%f2,%f18		! 3/ 7/ 5:Tmp=X[0,1]>>>24
	sll		%l2,5,%i3			!! 3
	and		%l4,%l3,%g5
	ld		[%o5+12],%o7
	 .word	0x85b08a42 !fpadd32	%f2,%f2,%f2		! 4/ 8/ 6:X[0,1]<<=1
	srl		%l2,27,%i4
	add		%i3,%l1,%l1
	 .word	0xa5b486e1 !fmul8ulx16	%f18,%f32,%f18			! 5/10/ 7:Tmp>>=7, Tmp&=1
	sll		%l3,30,%i5
	add		%i4,%l1,%l1
	 .word	0xa9b0ca40 !fpadd32	%f34,%f0,%f20			!
	andn		%l0,%l3,%i4
	add		%o7,%l1,%l1
	 .word	0x89b04da4 !fxors	%f1,%f4,%f4	!-1/-1/-1:X[0]^=X[13]
	srl		%l3,2,%l3
	or		%i4,%g5,%i4
	 .word	0x85b48d82 !fxor	%f18,%f2,%f2		! 8/14/10:X[0,1]|=Tmp
	or		%i5,%l3,%l3
	add		%i4,%l1,%l1
	 std		%f20,[%o5+0]		!
	sll		%l1,5,%i3			!! 4
	and		%l3,%l2,%g5
	ld		[%o5+16],%o7
	 .word	0x8bb08da5 !fxors	%f2,%f5,%f5! 0/ 0/ 0:X[1]^=X[14]
	srl		%l1,27,%i4
	add		%i3,%l0,%l0
	 .word	0xa5b18d8c !fxor	%f6,%f12,%f18! 1/ 1/ 1:Tmp=X[2,3]^X[8,9]
	sll		%l2,30,%i5
	add		%i4,%l0,%l0
	andn		%l4,%l2,%i4
	add		%o7,%l0,%l0
	 .word	0x89b48d84 !fxor	%f18,%f4,%f4		! 2/ 4/ 3:X[0,1]^=X[2,3]^X[8,9]
	srl		%l2,2,%l2
	or		%i4,%g5,%i4
	or		%i5,%l2,%l2
	add		%i4,%l0,%l0
	 .word	0xa5b10904 !faligndata	%f4,%f4,%f18		! 3/ 7/ 5:Tmp=X[0,1]>>>24
	sll		%l0,5,%i3			!! 5
	and		%l2,%l1,%g5
	ld		[%o5+20],%o7
	 .word	0x89b10a44 !fpadd32	%f4,%f4,%f4		! 4/ 8/ 6:X[0,1]<<=1
	srl		%l0,27,%i4
	add		%i3,%l4,%l4
	 .word	0xa5b486e1 !fmul8ulx16	%f18,%f32,%f18			! 5/10/ 7:Tmp>>=7, Tmp&=1
	sll		%l1,30,%i5
	add		%i4,%l4,%l4
	 .word	0xa9b0ca42 !fpadd32	%f34,%f2,%f20			!
	andn		%l3,%l1,%i4
	add		%o7,%l4,%l4
	 .word	0x8db0cda6 !fxors	%f3,%f6,%f6	!-1/-1/-1:X[0]^=X[13]
	srl		%l1,2,%l1
	or		%i4,%g5,%i4
	 .word	0x89b48d84 !fxor	%f18,%f4,%f4		! 8/14/10:X[0,1]|=Tmp
	or		%i5,%l1,%l1
	add		%i4,%l4,%l4
	 std		%f20,[%o5+8]		!
	sll		%l4,5,%i3			!! 6
	and		%l1,%l0,%g5
	ld		[%o5+24],%o7
	 .word	0x8fb10da7 !fxors	%f4,%f7,%f7! 0/ 0/ 0:X[1]^=X[14]
	srl		%l4,27,%i4
	add		%i3,%l3,%l3
	 .word	0xa5b20d8e !fxor	%f8,%f14,%f18! 1/ 1/ 1:Tmp=X[2,3]^X[8,9]
	sll		%l0,30,%i5
	add		%i4,%l3,%l3
	andn		%l2,%l0,%i4
	add		%o7,%l3,%l3
	 .word	0x8db48d86 !fxor	%f18,%f6,%f6		! 2/ 4/ 3:X[0,1]^=X[2,3]^X[8,9]
	srl		%l0,2,%l0
	or		%i4,%g5,%i4
	or		%i5,%l0,%l0
	add		%i4,%l3,%l3
	 .word	0xa5b18906 !faligndata	%f6,%f6,%f18		! 3/ 7/ 5:Tmp=X[0,1]>>>24
	sll		%l3,5,%i3			!! 7
	and		%l0,%l4,%g5
	ld		[%o5+28],%o7
	 .word	0x8db18a46 !fpadd32	%f6,%f6,%f6		! 4/ 8/ 6:X[0,1]<<=1
	srl		%l3,27,%i4
	add		%i3,%l2,%l2
	 .word	0xa5b486e1 !fmul8ulx16	%f18,%f32,%f18			! 5/10/ 7:Tmp>>=7, Tmp&=1
	sll		%l4,30,%i5
	add		%i4,%l2,%l2
	 .word	0xa9b14a44 !fpadd32	%f36,%f4,%f20			!
	andn		%l1,%l4,%i4
	add		%o7,%l2,%l2
	 .word	0x91b14da8 !fxors	%f5,%f8,%f8	!-1/-1/-1:X[0]^=X[13]
	srl		%l4,2,%l4
	or		%i4,%g5,%i4
	 .word	0x8db48d86 !fxor	%f18,%f6,%f6		! 8/14/10:X[0,1]|=Tmp
	or		%i5,%l4,%l4
	add		%i4,%l2,%l2
	 std		%f20,[%o5+16]		!
	sll		%l2,5,%i3			!! 8
	and		%l4,%l3,%g5
	ld		[%o5+32],%o7
	 .word	0x93b18da9 !fxors	%f6,%f9,%f9! 0/ 0/ 0:X[1]^=X[14]
	srl		%l2,27,%i4
	add		%i3,%l1,%l1
	 .word	0xa5b28d80 !fxor	%f10,%f0,%f18! 1/ 1/ 1:Tmp=X[2,3]^X[8,9]
	sll		%l3,30,%i5
	add		%i4,%l1,%l1
	andn		%l0,%l3,%i4
	add		%o7,%l1,%l1
	 .word	0x91b48d88 !fxor	%f18,%f8,%f8		! 2/ 4/ 3:X[0,1]^=X[2,3]^X[8,9]
	srl		%l3,2,%l3
	or		%i4,%g5,%i4
	or		%i5,%l3,%l3
	add		%i4,%l1,%l1
	 .word	0xa5b20908 !faligndata	%f8,%f8,%f18		! 3/ 7/ 5:Tmp=X[0,1]>>>24
	sll		%l1,5,%i3			!! 9
	and		%l3,%l2,%g5
	ld		[%o5+36],%o7
	 .word	0x91b20a48 !fpadd32	%f8,%f8,%f8		! 4/ 8/ 6:X[0,1]<<=1
	srl		%l1,27,%i4
	add		%i3,%l0,%l0
	 .word	0xa5b486e1 !fmul8ulx16	%f18,%f32,%f18			! 5/10/ 7:Tmp>>=7, Tmp&=1
	sll		%l2,30,%i5
	add		%i4,%l0,%l0
	 .word	0xa9b14a46 !fpadd32	%f36,%f6,%f20			!
	andn		%l4,%l2,%i4
	add		%o7,%l0,%l0
	 .word	0x95b1cdaa !fxors	%f7,%f10,%f10	!-1/-1/-1:X[0]^=X[13]
	srl		%l2,2,%l2
	or		%i4,%g5,%i4
	 .word	0x91b48d88 !fxor	%f18,%f8,%f8		! 8/14/10:X[0,1]|=Tmp
	or		%i5,%l2,%l2
	add		%i4,%l0,%l0
	 std		%f20,[%o5+24]		!
	sll		%l0,5,%i3			!! 10
	and		%l2,%l1,%g5
	ld		[%o5+40],%o7
	 .word	0x97b20dab !fxors	%f8,%f11,%f11! 0/ 0/ 0:X[1]^=X[14]
	srl		%l0,27,%i4
	add		%i3,%l4,%l4
	 .word	0xa5b30d82 !fxor	%f12,%f2,%f18! 1/ 1/ 1:Tmp=X[2,3]^X[8,9]
	sll		%l1,30,%i5
	add		%i4,%l4,%l4
	andn		%l3,%l1,%i4
	add		%o7,%l4,%l4
	 .word	0x95b48d8a !fxor	%f18,%f10,%f10		! 2/ 4/ 3:X[0,1]^=X[2,3]^X[8,9]
	srl		%l1,2,%l1
	or		%i4,%g5,%i4
	or		%i5,%l1,%l1
	add		%i4,%l4,%l4
	 .word	0xa5b2890a !faligndata	%f10,%f10,%f18		! 3/ 7/ 5:Tmp=X[0,1]>>>24
	sll		%l4,5,%i3			!! 11
	and		%l1,%l0,%g5
	ld		[%o5+44],%o7
	 .word	0x95b28a4a !fpadd32	%f10,%f10,%f10		! 4/ 8/ 6:X[0,1]<<=1
	srl		%l4,27,%i4
	add		%i3,%l3,%l3
	 .word	0xa5b486e1 !fmul8ulx16	%f18,%f32,%f18			! 5/10/ 7:Tmp>>=7, Tmp&=1
	sll		%l0,30,%i5
	add		%i4,%l3,%l3
	 .word	0xa9b14a48 !fpadd32	%f36,%f8,%f20			!
	andn		%l2,%l0,%i4
	add		%o7,%l3,%l3
	 .word	0x99b24dac !fxors	%f9,%f12,%f12	!-1/-1/-1:X[0]^=X[13]
	srl		%l0,2,%l0
	or		%i4,%g5,%i4
	 .word	0x95b48d8a !fxor	%f18,%f10,%f10		! 8/14/10:X[0,1]|=Tmp
	or		%i5,%l0,%l0
	add		%i4,%l3,%l3
	 std		%f20,[%o5+32]		!
	sll		%l3,5,%i3			!! 12
	and		%l0,%l4,%g5
	ld		[%o5+48],%o7
	 .word	0x9bb28dad !fxors	%f10,%f13,%f13! 0/ 0/ 0:X[1]^=X[14]
	srl		%l3,27,%i4
	add		%i3,%l2,%l2
	 .word	0xa5b38d84 !fxor	%f14,%f4,%f18! 1/ 1/ 1:Tmp=X[2,3]^X[8,9]
	sll		%l4,30,%i5
	add		%i4,%l2,%l2
	andn		%l1,%l4,%i4
	add		%o7,%l2,%l2
	 .word	0x99b48d8c !fxor	%f18,%f12,%f12		! 2/ 4/ 3:X[0,1]^=X[2,3]^X[8,9]
	srl		%l4,2,%l4
	or		%i4,%g5,%i4
	or		%i5,%l4,%l4
	add		%i4,%l2,%l2
	 .word	0xa5b3090c !faligndata	%f12,%f12,%f18		! 3/ 7/ 5:Tmp=X[0,1]>>>24
	sll		%l2,5,%i3			!! 13
	and		%l4,%l3,%g5
	ld		[%o5+52],%o7
	 .word	0x99b30a4c !fpadd32	%f12,%f12,%f12		! 4/ 8/ 6:X[0,1]<<=1
	srl		%l2,27,%i4
	add		%i3,%l1,%l1
	 .word	0xa5b486e1 !fmul8ulx16	%f18,%f32,%f18			! 5/10/ 7:Tmp>>=7, Tmp&=1
	sll		%l3,30,%i5
	add		%i4,%l1,%l1
	 .word	0xa9b14a4a !fpadd32	%f36,%f10,%f20			!
	andn		%l0,%l3,%i4
	add		%o7,%l1,%l1
	 .word	0x9db2cdae !fxors	%f11,%f14,%f14	!-1/-1/-1:X[0]^=X[13]
	srl		%l3,2,%l3
	or		%i4,%g5,%i4
	 .word	0x99b48d8c !fxor	%f18,%f12,%f12		! 8/14/10:X[0,1]|=Tmp
	or		%i5,%l3,%l3
	add		%i4,%l1,%l1
	 std		%f20,[%o5+40]		!
	sll		%l1,5,%i3			!! 14
	and		%l3,%l2,%g5
	ld		[%o5+56],%o7
	 .word	0x9fb30daf !fxors	%f12,%f15,%f15! 0/ 0/ 0:X[1]^=X[14]
	srl		%l1,27,%i4
	add		%i3,%l0,%l0
	 .word	0xa5b00d86 !fxor	%f0,%f6,%f18! 1/ 1/ 1:Tmp=X[2,3]^X[8,9]
	sll		%l2,30,%i5
	add		%i4,%l0,%l0
	andn		%l4,%l2,%i4
	add		%o7,%l0,%l0
	 .word	0x9db48d8e !fxor	%f18,%f14,%f14		! 2/ 4/ 3:X[0,1]^=X[2,3]^X[8,9]
	srl		%l2,2,%l2
	or		%i4,%g5,%i4
	or		%i5,%l2,%l2
	add		%i4,%l0,%l0
	 .word	0xa5b3890e !faligndata	%f14,%f14,%f18		! 3/ 7/ 5:Tmp=X[0,1]>>>24
	sll		%l0,5,%i3			!! 15
	and		%l2,%l1,%g5
	ld		[%o5+60],%o7
	 .word	0x9db38a4e !fpadd32	%f14,%f14,%f14		! 4/ 8/ 6:X[0,1]<<=1
	srl		%l0,27,%i4
	add		%i3,%l4,%l4
	 .word	0xa5b486e1 !fmul8ulx16	%f18,%f32,%f18			! 5/10/ 7:Tmp>>=7, Tmp&=1
	sll		%l1,30,%i5
	add		%i4,%l4,%l4
	 .word	0xa9b14a4c !fpadd32	%f36,%f12,%f20			!
	andn		%l3,%l1,%i4
	add		%o7,%l4,%l4
	 .word	0x81b34da0 !fxors	%f13,%f0,%f0	!-1/-1/-1:X[0]^=X[13]
	srl		%l1,2,%l1
	or		%i4,%g5,%i4
	 .word	0x9db48d8e !fxor	%f18,%f14,%f14		! 8/14/10:X[0,1]|=Tmp
	or		%i5,%l1,%l1
	add		%i4,%l4,%l4
	 std		%f20,[%o5+48]		!
	sll		%l4,5,%i3			!! 16
	and		%l1,%l0,%g5
	ld		[%o5+0],%o7
	 .word	0x83b38da1 !fxors	%f14,%f1,%f1! 0/ 0/ 0:X[1]^=X[14]
	srl		%l4,27,%i4
	add		%i3,%l3,%l3
	 .word	0xa5b08d88 !fxor	%f2,%f8,%f18! 1/ 1/ 1:Tmp=X[2,3]^X[8,9]
	sll		%l0,30,%i5
	add		%i4,%l3,%l3
	andn		%l2,%l0,%i4
	add		%o7,%l3,%l3
	 .word	0x81b48d80 !fxor	%f18,%f0,%f0		! 2/ 4/ 3:X[0,1]^=X[2,3]^X[8,9]
	srl		%l0,2,%l0
	or		%i4,%g5,%i4
	or		%i5,%l0,%l0
	add		%i4,%l3,%l3
	 .word	0xa5b00900 !faligndata	%f0,%f0,%f18		! 3/ 7/ 5:Tmp=X[0,1]>>>24
	sll		%l3,5,%i3			!! 17
	and		%l0,%l4,%g5
	ld		[%o5+4],%o7
	 .word	0x81b00a40 !fpadd32	%f0,%f0,%f0		! 4/ 8/ 6:X[0,1]<<=1
	srl		%l3,27,%i4
	add		%i3,%l2,%l2
	 .word	0xa5b486e1 !fmul8ulx16	%f18,%f32,%f18			! 5/10/ 7:Tmp>>=7, Tmp&=1
	sll		%l4,30,%i5
	add		%i4,%l2,%l2
	 .word	0xa9b14a4e !fpadd32	%f36,%f14,%f20			!
	andn		%l1,%l4,%i4
	add		%o7,%l2,%l2
	 .word	0x85b3cda2 !fxors	%f15,%f2,%f2	!-1/-1/-1:X[0]^=X[13]
	srl		%l4,2,%l4
	or		%i4,%g5,%i4
	 .word	0x81b48d80 !fxor	%f18,%f0,%f0		! 8/14/10:X[0,1]|=Tmp
	or		%i5,%l4,%l4
	add		%i4,%l2,%l2
	 std		%f20,[%o5+56]		!
	sll		%l2,5,%i3			!! 18
	and		%l4,%l3,%g5
	ld		[%o5+8],%o7
	 .word	0x87b00da3 !fxors	%f0,%f3,%f3! 0/ 0/ 0:X[1]^=X[14]
	srl		%l2,27,%i4
	add		%i3,%l1,%l1
	 .word	0xa5b10d8a !fxor	%f4,%f10,%f18! 1/ 1/ 1:Tmp=X[2,3]^X[8,9]
	sll		%l3,30,%i5
	add		%i4,%l1,%l1
	andn		%l0,%l3,%i4
	add		%o7,%l1,%l1
	 .word	0x85b48d82 !fxor	%f18,%f2,%f2		! 2/ 4/ 3:X[0,1]^=X[2,3]^X[8,9]
	srl		%l3,2,%l3
	or		%i4,%g5,%i4
	or		%i5,%l3,%l3
	add		%i4,%l1,%l1
	 .word	0xa5b08902 !faligndata	%f2,%f2,%f18		! 3/ 7/ 5:Tmp=X[0,1]>>>24
	sll		%l1,5,%i3			!! 19
	and		%l3,%l2,%g5
	ld		[%o5+12],%o7
	 .word	0x85b08a42 !fpadd32	%f2,%f2,%f2		! 4/ 8/ 6:X[0,1]<<=1
	srl		%l1,27,%i4
	add		%i3,%l0,%l0
	 .word	0xa5b486e1 !fmul8ulx16	%f18,%f32,%f18			! 5/10/ 7:Tmp>>=7, Tmp&=1
	sll		%l2,30,%i5
	add		%i4,%l0,%l0
	 .word	0xa9b14a40 !fpadd32	%f36,%f0,%f20			!
	andn		%l4,%l2,%i4
	add		%o7,%l0,%l0
	 .word	0x89b04da4 !fxors	%f1,%f4,%f4	!-1/-1/-1:X[0]^=X[13]
	srl		%l2,2,%l2
	or		%i4,%g5,%i4
	 .word	0x85b48d82 !fxor	%f18,%f2,%f2		! 8/14/10:X[0,1]|=Tmp
	or		%i5,%l2,%l2
	add		%i4,%l0,%l0
	 std		%f20,[%o5+0]		!
	sll		%l0,5,%i3			!! 20
	ld		[%o5+16],%o7
	 .word	0x8bb08da5 !fxors	%f2,%f5,%f5! 0/ 0/ 0:X[1]^=X[14]
	srl		%l0,27,%i4
	add		%i3,%l4,%l4
	 .word	0xa5b18d8c !fxor	%f6,%f12,%f18! 1/ 1/ 1:Tmp=X[2,3]^X[8,9]
	xor		%l2,%l1,%i3
	add		%i4,%l4,%l4
	sll		%l1,30,%i5
	xor		%l3,%i3,%i4
	 .word	0x89b48d84 !fxor	%f18,%f4,%f4		! 2/ 4/ 3:X[0,1]^=X[2,3]^X[8,9]
	srl		%l1,2,%l1
	add		%i4,%l4,%l4
	or		%i5,%l1,%l1
	add		%o7,%l4,%l4
	 .word	0xa5b10904 !faligndata	%f4,%f4,%f18		! 3/ 7/ 5:Tmp=X[0,1]>>>24
	sll		%l4,5,%i3			!! 21
	ld		[%o5+20],%o7
	 .word	0x89b10a44 !fpadd32	%f4,%f4,%f4		! 4/ 8/ 6:X[0,1]<<=1
	srl		%l4,27,%i4
	add		%i3,%l3,%l3
	 .word	0xa5b486e1 !fmul8ulx16	%f18,%f32,%f18			! 5/10/ 7:Tmp>>=7, Tmp&=1
	xor		%l1,%l0,%i3
	add		%i4,%l3,%l3
	 .word	0xa9b14a42 !fpadd32	%f36,%f2,%f20			!
	sll		%l0,30,%i5
	xor		%l2,%i3,%i4
	 .word	0x8db0cda6 !fxors	%f3,%f6,%f6	!-1/-1/-1:X[0]^=X[13]
	srl		%l0,2,%l0
	add		%i4,%l3,%l3
	 .word	0x89b48d84 !fxor	%f18,%f4,%f4		! 8/14/10:X[0,1]|=Tmp
	or		%i5,%l0,%l0
	add		%o7,%l3,%l3
	 std		%f20,[%o5+8]		!
	sll		%l3,5,%i3			!! 22
	ld		[%o5+24],%o7
	 .word	0x8fb10da7 !fxors	%f4,%f7,%f7! 0/ 0/ 0:X[1]^=X[14]
	srl		%l3,27,%i4
	add		%i3,%l2,%l2
	 .word	0xa5b20d8e !fxor	%f8,%f14,%f18! 1/ 1/ 1:Tmp=X[2,3]^X[8,9]
	xor		%l0,%l4,%i3
	add		%i4,%l2,%l2
	sll		%l4,30,%i5
	xor		%l1,%i3,%i4
	 .word	0x8db48d86 !fxor	%f18,%f6,%f6		! 2/ 4/ 3:X[0,1]^=X[2,3]^X[8,9]
	srl		%l4,2,%l4
	add		%i4,%l2,%l2
	or		%i5,%l4,%l4
	add		%o7,%l2,%l2
	 .word	0xa5b18906 !faligndata	%f6,%f6,%f18		! 3/ 7/ 5:Tmp=X[0,1]>>>24
	sll		%l2,5,%i3			!! 23
	ld		[%o5+28],%o7
	 .word	0x8db18a46 !fpadd32	%f6,%f6,%f6		! 4/ 8/ 6:X[0,1]<<=1
	srl		%l2,27,%i4
	add		%i3,%l1,%l1
	 .word	0xa5b486e1 !fmul8ulx16	%f18,%f32,%f18			! 5/10/ 7:Tmp>>=7, Tmp&=1
	xor		%l4,%l3,%i3
	add		%i4,%l1,%l1
	 .word	0xa9b14a44 !fpadd32	%f36,%f4,%f20			!
	sll		%l3,30,%i5
	xor		%l0,%i3,%i4
	 .word	0x91b14da8 !fxors	%f5,%f8,%f8	!-1/-1/-1:X[0]^=X[13]
	srl		%l3,2,%l3
	add		%i4,%l1,%l1
	 .word	0x8db48d86 !fxor	%f18,%f6,%f6		! 8/14/10:X[0,1]|=Tmp
	or		%i5,%l3,%l3
	add		%o7,%l1,%l1
	 std		%f20,[%o5+16]		!
	sll		%l1,5,%i3			!! 24
	ld		[%o5+32],%o7
	 .word	0x93b18da9 !fxors	%f6,%f9,%f9! 0/ 0/ 0:X[1]^=X[14]
	srl		%l1,27,%i4
	add		%i3,%l0,%l0
	 .word	0xa5b28d80 !fxor	%f10,%f0,%f18! 1/ 1/ 1:Tmp=X[2,3]^X[8,9]
	xor		%l3,%l2,%i3
	add		%i4,%l0,%l0
	sll		%l2,30,%i5
	xor		%l4,%i3,%i4
	 .word	0x91b48d88 !fxor	%f18,%f8,%f8		! 2/ 4/ 3:X[0,1]^=X[2,3]^X[8,9]
	srl		%l2,2,%l2
	add		%i4,%l0,%l0
	or		%i5,%l2,%l2
	add		%o7,%l0,%l0
	 .word	0xa5b20908 !faligndata	%f8,%f8,%f18		! 3/ 7/ 5:Tmp=X[0,1]>>>24
	sll		%l0,5,%i3			!! 25
	ld		[%o5+36],%o7
	 .word	0x91b20a48 !fpadd32	%f8,%f8,%f8		! 4/ 8/ 6:X[0,1]<<=1
	srl		%l0,27,%i4
	add		%i3,%l4,%l4
	 .word	0xa5b486e1 !fmul8ulx16	%f18,%f32,%f18			! 5/10/ 7:Tmp>>=7, Tmp&=1
	xor		%l2,%l1,%i3
	add		%i4,%l4,%l4
	 .word	0xa9b14a46 !fpadd32	%f36,%f6,%f20			!
	sll		%l1,30,%i5
	xor		%l3,%i3,%i4
	 .word	0x95b1cdaa !fxors	%f7,%f10,%f10	!-1/-1/-1:X[0]^=X[13]
	srl		%l1,2,%l1
	add		%i4,%l4,%l4
	 .word	0x91b48d88 !fxor	%f18,%f8,%f8		! 8/14/10:X[0,1]|=Tmp
	or		%i5,%l1,%l1
	add		%o7,%l4,%l4
	 std		%f20,[%o5+24]		!
	sll		%l4,5,%i3			!! 26
	ld		[%o5+40],%o7
	 .word	0x97b20dab !fxors	%f8,%f11,%f11! 0/ 0/ 0:X[1]^=X[14]
	srl		%l4,27,%i4
	add		%i3,%l3,%l3
	 .word	0xa5b30d82 !fxor	%f12,%f2,%f18! 1/ 1/ 1:Tmp=X[2,3]^X[8,9]
	xor		%l1,%l0,%i3
	add		%i4,%l3,%l3
	sll		%l0,30,%i5
	xor		%l2,%i3,%i4
	 .word	0x95b48d8a !fxor	%f18,%f10,%f10		! 2/ 4/ 3:X[0,1]^=X[2,3]^X[8,9]
	srl		%l0,2,%l0
	add		%i4,%l3,%l3
	or		%i5,%l0,%l0
	add		%o7,%l3,%l3
	 .word	0xa5b2890a !faligndata	%f10,%f10,%f18		! 3/ 7/ 5:Tmp=X[0,1]>>>24
	sll		%l3,5,%i3			!! 27
	ld		[%o5+44],%o7
	 .word	0x95b28a4a !fpadd32	%f10,%f10,%f10		! 4/ 8/ 6:X[0,1]<<=1
	srl		%l3,27,%i4
	add		%i3,%l2,%l2
	 .word	0xa5b486e1 !fmul8ulx16	%f18,%f32,%f18			! 5/10/ 7:Tmp>>=7, Tmp&=1
	xor		%l0,%l4,%i3
	add		%i4,%l2,%l2
	 .word	0xa9b1ca48 !fpadd32	%f38,%f8,%f20			!
	sll		%l4,30,%i5
	xor		%l1,%i3,%i4
	 .word	0x99b24dac !fxors	%f9,%f12,%f12	!-1/-1/-1:X[0]^=X[13]
	srl		%l4,2,%l4
	add		%i4,%l2,%l2
	 .word	0x95b48d8a !fxor	%f18,%f10,%f10		! 8/14/10:X[0,1]|=Tmp
	or		%i5,%l4,%l4
	add		%o7,%l2,%l2
	 std		%f20,[%o5+32]		!
	sll		%l2,5,%i3			!! 28
	ld		[%o5+48],%o7
	 .word	0x9bb28dad !fxors	%f10,%f13,%f13! 0/ 0/ 0:X[1]^=X[14]
	srl		%l2,27,%i4
	add		%i3,%l1,%l1
	 .word	0xa5b38d84 !fxor	%f14,%f4,%f18! 1/ 1/ 1:Tmp=X[2,3]^X[8,9]
	xor		%l4,%l3,%i3
	add		%i4,%l1,%l1
	sll		%l3,30,%i5
	xor		%l0,%i3,%i4
	 .word	0x99b48d8c !fxor	%f18,%f12,%f12		! 2/ 4/ 3:X[0,1]^=X[2,3]^X[8,9]
	srl		%l3,2,%l3
	add		%i4,%l1,%l1
	or		%i5,%l3,%l3
	add		%o7,%l1,%l1
	 .word	0xa5b3090c !faligndata	%f12,%f12,%f18		! 3/ 7/ 5:Tmp=X[0,1]>>>24
	sll		%l1,5,%i3			!! 29
	ld		[%o5+52],%o7
	 .word	0x99b30a4c !fpadd32	%f12,%f12,%f12		! 4/ 8/ 6:X[0,1]<<=1
	srl		%l1,27,%i4
	add		%i3,%l0,%l0
	 .word	0xa5b486e1 !fmul8ulx16	%f18,%f32,%f18			! 5/10/ 7:Tmp>>=7, Tmp&=1
	xor		%l3,%l2,%i3
	add		%i4,%l0,%l0
	 .word	0xa9b1ca4a !fpadd32	%f38,%f10,%f20			!
	sll		%l2,30,%i5
	xor		%l4,%i3,%i4
	 .word	0x9db2cdae !fxors	%f11,%f14,%f14	!-1/-1/-1:X[0]^=X[13]
	srl		%l2,2,%l2
	add		%i4,%l0,%l0
	 .word	0x99b48d8c !fxor	%f18,%f12,%f12		! 8/14/10:X[0,1]|=Tmp
	or		%i5,%l2,%l2
	add		%o7,%l0,%l0
	 std		%f20,[%o5+40]		!
	sll		%l0,5,%i3			!! 30
	ld		[%o5+56],%o7
	 .word	0x9fb30daf !fxors	%f12,%f15,%f15! 0/ 0/ 0:X[1]^=X[14]
	srl		%l0,27,%i4
	add		%i3,%l4,%l4
	 .word	0xa5b00d86 !fxor	%f0,%f6,%f18! 1/ 1/ 1:Tmp=X[2,3]^X[8,9]
	xor		%l2,%l1,%i3
	add		%i4,%l4,%l4
	sll		%l1,30,%i5
	xor		%l3,%i3,%i4
	 .word	0x9db48d8e !fxor	%f18,%f14,%f14		! 2/ 4/ 3:X[0,1]^=X[2,3]^X[8,9]
	srl		%l1,2,%l1
	add		%i4,%l4,%l4
	or		%i5,%l1,%l1
	add		%o7,%l4,%l4
	 .word	0xa5b3890e !faligndata	%f14,%f14,%f18		! 3/ 7/ 5:Tmp=X[0,1]>>>24
	sll		%l4,5,%i3			!! 31
	ld		[%o5+60],%o7
	 .word	0x9db38a4e !fpadd32	%f14,%f14,%f14		! 4/ 8/ 6:X[0,1]<<=1
	srl		%l4,27,%i4
	add		%i3,%l3,%l3
	 .word	0xa5b486e1 !fmul8ulx16	%f18,%f32,%f18			! 5/10/ 7:Tmp>>=7, Tmp&=1
	xor		%l1,%l0,%i3
	add		%i4,%l3,%l3
	 .word	0xa9b1ca4c !fpadd32	%f38,%f12,%f20			!
	sll		%l0,30,%i5
	xor		%l2,%i3,%i4
	 .word	0x81b34da0 !fxors	%f13,%f0,%f0	!-1/-1/-1:X[0]^=X[13]
	srl		%l0,2,%l0
	add		%i4,%l3,%l3
	 .word	0x9db48d8e !fxor	%f18,%f14,%f14		! 8/14/10:X[0,1]|=Tmp
	or		%i5,%l0,%l0
	add		%o7,%l3,%l3
	 std		%f20,[%o5+48]		!
	sll		%l3,5,%i3			!! 32
	ld		[%o5+0],%o7
	 .word	0x83b38da1 !fxors	%f14,%f1,%f1! 0/ 0/ 0:X[1]^=X[14]
	srl		%l3,27,%i4
	add		%i3,%l2,%l2
	 .word	0xa5b08d88 !fxor	%f2,%f8,%f18! 1/ 1/ 1:Tmp=X[2,3]^X[8,9]
	xor		%l0,%l4,%i3
	add		%i4,%l2,%l2
	sll		%l4,30,%i5
	xor		%l1,%i3,%i4
	 .word	0x81b48d80 !fxor	%f18,%f0,%f0		! 2/ 4/ 3:X[0,1]^=X[2,3]^X[8,9]
	srl		%l4,2,%l4
	add		%i4,%l2,%l2
	or		%i5,%l4,%l4
	add		%o7,%l2,%l2
	 .word	0xa5b00900 !faligndata	%f0,%f0,%f18		! 3/ 7/ 5:Tmp=X[0,1]>>>24
	sll		%l2,5,%i3			!! 33
	ld		[%o5+4],%o7
	 .word	0x81b00a40 !fpadd32	%f0,%f0,%f0		! 4/ 8/ 6:X[0,1]<<=1
	srl		%l2,27,%i4
	add		%i3,%l1,%l1
	 .word	0xa5b486e1 !fmul8ulx16	%f18,%f32,%f18			! 5/10/ 7:Tmp>>=7, Tmp&=1
	xor		%l4,%l3,%i3
	add		%i4,%l1,%l1
	 .word	0xa9b1ca4e !fpadd32	%f38,%f14,%f20			!
	sll		%l3,30,%i5
	xor		%l0,%i3,%i4
	 .word	0x85b3cda2 !fxors	%f15,%f2,%f2	!-1/-1/-1:X[0]^=X[13]
	srl		%l3,2,%l3
	add		%i4,%l1,%l1
	 .word	0x81b48d80 !fxor	%f18,%f0,%f0		! 8/14/10:X[0,1]|=Tmp
	or		%i5,%l3,%l3
	add		%o7,%l1,%l1
	 std		%f20,[%o5+56]		!
	sll		%l1,5,%i3			!! 34
	ld		[%o5+8],%o7
	 .word	0x87b00da3 !fxors	%f0,%f3,%f3! 0/ 0/ 0:X[1]^=X[14]
	srl		%l1,27,%i4
	add		%i3,%l0,%l0
	 .word	0xa5b10d8a !fxor	%f4,%f10,%f18! 1/ 1/ 1:Tmp=X[2,3]^X[8,9]
	xor		%l3,%l2,%i3
	add		%i4,%l0,%l0
	sll		%l2,30,%i5
	xor		%l4,%i3,%i4
	 .word	0x85b48d82 !fxor	%f18,%f2,%f2		! 2/ 4/ 3:X[0,1]^=X[2,3]^X[8,9]
	srl		%l2,2,%l2
	add		%i4,%l0,%l0
	or		%i5,%l2,%l2
	add		%o7,%l0,%l0
	 .word	0xa5b08902 !faligndata	%f2,%f2,%f18		! 3/ 7/ 5:Tmp=X[0,1]>>>24
	sll		%l0,5,%i3			!! 35
	ld		[%o5+12],%o7
	 .word	0x85b08a42 !fpadd32	%f2,%f2,%f2		! 4/ 8/ 6:X[0,1]<<=1
	srl		%l0,27,%i4
	add		%i3,%l4,%l4
	 .word	0xa5b486e1 !fmul8ulx16	%f18,%f32,%f18			! 5/10/ 7:Tmp>>=7, Tmp&=1
	xor		%l2,%l1,%i3
	add		%i4,%l4,%l4
	 .word	0xa9b1ca40 !fpadd32	%f38,%f0,%f20			!
	sll		%l1,30,%i5
	xor		%l3,%i3,%i4
	 .word	0x89b04da4 !fxors	%f1,%f4,%f4	!-1/-1/-1:X[0]^=X[13]
	srl		%l1,2,%l1
	add		%i4,%l4,%l4
	 .word	0x85b48d82 !fxor	%f18,%f2,%f2		! 8/14/10:X[0,1]|=Tmp
	or		%i5,%l1,%l1
	add		%o7,%l4,%l4
	 std		%f20,[%o5+0]		!
	sll		%l4,5,%i3			!! 36
	ld		[%o5+16],%o7
	 .word	0x8bb08da5 !fxors	%f2,%f5,%f5! 0/ 0/ 0:X[1]^=X[14]
	srl		%l4,27,%i4
	add		%i3,%l3,%l3
	 .word	0xa5b18d8c !fxor	%f6,%f12,%f18! 1/ 1/ 1:Tmp=X[2,3]^X[8,9]
	xor		%l1,%l0,%i3
	add		%i4,%l3,%l3
	sll		%l0,30,%i5
	xor		%l2,%i3,%i4
	 .word	0x89b48d84 !fxor	%f18,%f4,%f4		! 2/ 4/ 3:X[0,1]^=X[2,3]^X[8,9]
	srl		%l0,2,%l0
	add		%i4,%l3,%l3
	or		%i5,%l0,%l0
	add		%o7,%l3,%l3
	 .word	0xa5b10904 !faligndata	%f4,%f4,%f18		! 3/ 7/ 5:Tmp=X[0,1]>>>24
	sll		%l3,5,%i3			!! 37
	ld		[%o5+20],%o7
	 .word	0x89b10a44 !fpadd32	%f4,%f4,%f4		! 4/ 8/ 6:X[0,1]<<=1
	srl		%l3,27,%i4
	add		%i3,%l2,%l2
	 .word	0xa5b486e1 !fmul8ulx16	%f18,%f32,%f18			! 5/10/ 7:Tmp>>=7, Tmp&=1
	xor		%l0,%l4,%i3
	add		%i4,%l2,%l2
	 .word	0xa9b1ca42 !fpadd32	%f38,%f2,%f20			!
	sll		%l4,30,%i5
	xor		%l1,%i3,%i4
	 .word	0x8db0cda6 !fxors	%f3,%f6,%f6	!-1/-1/-1:X[0]^=X[13]
	srl		%l4,2,%l4
	add		%i4,%l2,%l2
	 .word	0x89b48d84 !fxor	%f18,%f4,%f4		! 8/14/10:X[0,1]|=Tmp
	or		%i5,%l4,%l4
	add		%o7,%l2,%l2
	 std		%f20,[%o5+8]		!
	sll		%l2,5,%i3			!! 38
	ld		[%o5+24],%o7
	 .word	0x8fb10da7 !fxors	%f4,%f7,%f7! 0/ 0/ 0:X[1]^=X[14]
	srl		%l2,27,%i4
	add		%i3,%l1,%l1
	 .word	0xa5b20d8e !fxor	%f8,%f14,%f18! 1/ 1/ 1:Tmp=X[2,3]^X[8,9]
	xor		%l4,%l3,%i3
	add		%i4,%l1,%l1
	sll		%l3,30,%i5
	xor		%l0,%i3,%i4
	 .word	0x8db48d86 !fxor	%f18,%f6,%f6		! 2/ 4/ 3:X[0,1]^=X[2,3]^X[8,9]
	srl		%l3,2,%l3
	add		%i4,%l1,%l1
	or		%i5,%l3,%l3
	add		%o7,%l1,%l1
	 .word	0xa5b18906 !faligndata	%f6,%f6,%f18		! 3/ 7/ 5:Tmp=X[0,1]>>>24
	sll		%l1,5,%i3			!! 39
	ld		[%o5+28],%o7
	 .word	0x8db18a46 !fpadd32	%f6,%f6,%f6		! 4/ 8/ 6:X[0,1]<<=1
	srl		%l1,27,%i4
	add		%i3,%l0,%l0
	 .word	0xa5b486e1 !fmul8ulx16	%f18,%f32,%f18			! 5/10/ 7:Tmp>>=7, Tmp&=1
	xor		%l3,%l2,%i3
	add		%i4,%l0,%l0
	 .word	0xa9b1ca44 !fpadd32	%f38,%f4,%f20			!
	sll		%l2,30,%i5
	xor		%l4,%i3,%i4
	 .word	0x91b14da8 !fxors	%f5,%f8,%f8	!-1/-1/-1:X[0]^=X[13]
	srl		%l2,2,%l2
	add		%i4,%l0,%l0
	 .word	0x8db48d86 !fxor	%f18,%f6,%f6		! 8/14/10:X[0,1]|=Tmp
	or		%i5,%l2,%l2
	add		%o7,%l0,%l0
	 std		%f20,[%o5+16]		!
	sll		%l0,5,%i3			!! 40
	ld		[%o5+32],%o7
	 .word	0x93b18da9 !fxors	%f6,%f9,%f9! 0/ 0/ 0:X[1]^=X[14]
	srl		%l0,27,%i4
	add		%i3,%l4,%l4
	 .word	0xa5b28d80 !fxor	%f10,%f0,%f18! 1/ 1/ 1:Tmp=X[2,3]^X[8,9]
	and		%l2,%l1,%i3
	add		%i4,%l4,%l4
	sll		%l1,30,%i5
	or		%l2,%l1,%i4
	 .word	0x91b48d88 !fxor	%f18,%f8,%f8		! 2/ 4/ 3:X[0,1]^=X[2,3]^X[8,9]
	srl		%l1,2,%l1
	and		%l3,%i4,%i4
	add		%o7,%l4,%l4
	or		%i4,%i3,%i4
	 .word	0xa5b20908 !faligndata	%f8,%f8,%f18		! 3/ 7/ 5:Tmp=X[0,1]>>>24
	or		%i5,%l1,%l1
	add		%i4,%l4,%l4
	 .word	0x91b20a48 !fpadd32	%f8,%f8,%f8		! 4/ 8/ 6:X[0,1]<<=1
	sll		%l4,5,%i3			!! 41
	ld		[%o5+36],%o7
	srl		%l4,27,%i4
	add		%i3,%l3,%l3
	 .word	0xa5b486e1 !fmul8ulx16	%f18,%f32,%f18			! 5/10/ 7:Tmp>>=7, Tmp&=1
	and		%l1,%l0,%i3
	add		%i4,%l3,%l3
	 .word	0xa9b1ca46 !fpadd32	%f38,%f6,%f20			!
	sll		%l0,30,%i5
	or		%l1,%l0,%i4
	 .word	0x95b1cdaa !fxors	%f7,%f10,%f10	!-1/-1/-1:X[0]^=X[13]
	srl		%l0,2,%l0
	and		%l2,%i4,%i4
	 .word	0x91b48d88 !fxor	%f18,%f8,%f8		! 8/14/10:X[0,1]|=Tmp
	add		%o7,%l3,%l3
	or		%i4,%i3,%i4
	or		%i5,%l0,%l0
	add		%i4,%l3,%l3
	 std		%f20,[%o5+24]		!
	sll		%l3,5,%i3			!! 42
	ld		[%o5+40],%o7
	 .word	0x97b20dab !fxors	%f8,%f11,%f11! 0/ 0/ 0:X[1]^=X[14]
	srl		%l3,27,%i4
	add		%i3,%l2,%l2
	 .word	0xa5b30d82 !fxor	%f12,%f2,%f18! 1/ 1/ 1:Tmp=X[2,3]^X[8,9]
	and		%l0,%l4,%i3
	add		%i4,%l2,%l2
	sll		%l4,30,%i5
	or		%l0,%l4,%i4
	 .word	0x95b48d8a !fxor	%f18,%f10,%f10		! 2/ 4/ 3:X[0,1]^=X[2,3]^X[8,9]
	srl		%l4,2,%l4
	and		%l1,%i4,%i4
	add		%o7,%l2,%l2
	or		%i4,%i3,%i4
	 .word	0xa5b2890a !faligndata	%f10,%f10,%f18		! 3/ 7/ 5:Tmp=X[0,1]>>>24
	or		%i5,%l4,%l4
	add		%i4,%l2,%l2
	 .word	0x95b28a4a !fpadd32	%f10,%f10,%f10		! 4/ 8/ 6:X[0,1]<<=1
	sll		%l2,5,%i3			!! 43
	ld		[%o5+44],%o7
	srl		%l2,27,%i4
	add		%i3,%l1,%l1
	 .word	0xa5b486e1 !fmul8ulx16	%f18,%f32,%f18			! 5/10/ 7:Tmp>>=7, Tmp&=1
	and		%l4,%l3,%i3
	add		%i4,%l1,%l1
	 .word	0xa9b1ca48 !fpadd32	%f38,%f8,%f20			!
	sll		%l3,30,%i5
	or		%l4,%l3,%i4
	 .word	0x99b24dac !fxors	%f9,%f12,%f12	!-1/-1/-1:X[0]^=X[13]
	srl		%l3,2,%l3
	and		%l0,%i4,%i4
	 .word	0x95b48d8a !fxor	%f18,%f10,%f10		! 8/14/10:X[0,1]|=Tmp
	add		%o7,%l1,%l1
	or		%i4,%i3,%i4
	or		%i5,%l3,%l3
	add		%i4,%l1,%l1
	 std		%f20,[%o5+32]		!
	sll		%l1,5,%i3			!! 44
	ld		[%o5+48],%o7
	 .word	0x9bb28dad !fxors	%f10,%f13,%f13! 0/ 0/ 0:X[1]^=X[14]
	srl		%l1,27,%i4
	add		%i3,%l0,%l0
	 .word	0xa5b38d84 !fxor	%f14,%f4,%f18! 1/ 1/ 1:Tmp=X[2,3]^X[8,9]
	and		%l3,%l2,%i3
	add		%i4,%l0,%l0
	sll		%l2,30,%i5
	or		%l3,%l2,%i4
	 .word	0x99b48d8c !fxor	%f18,%f12,%f12		! 2/ 4/ 3:X[0,1]^=X[2,3]^X[8,9]
	srl		%l2,2,%l2
	and		%l4,%i4,%i4
	add		%o7,%l0,%l0
	or		%i4,%i3,%i4
	 .word	0xa5b3090c !faligndata	%f12,%f12,%f18		! 3/ 7/ 5:Tmp=X[0,1]>>>24
	or		%i5,%l2,%l2
	add		%i4,%l0,%l0
	 .word	0x99b30a4c !fpadd32	%f12,%f12,%f12		! 4/ 8/ 6:X[0,1]<<=1
	sll		%l0,5,%i3			!! 45
	ld		[%o5+52],%o7
	srl		%l0,27,%i4
	add		%i3,%l4,%l4
	 .word	0xa5b486e1 !fmul8ulx16	%f18,%f32,%f18			! 5/10/ 7:Tmp>>=7, Tmp&=1
	and		%l2,%l1,%i3
	add		%i4,%l4,%l4
	 .word	0xa9b1ca4a !fpadd32	%f38,%f10,%f20			!
	sll		%l1,30,%i5
	or		%l2,%l1,%i4
	 .word	0x9db2cdae !fxors	%f11,%f14,%f14	!-1/-1/-1:X[0]^=X[13]
	srl		%l1,2,%l1
	and		%l3,%i4,%i4
	 .word	0x99b48d8c !fxor	%f18,%f12,%f12		! 8/14/10:X[0,1]|=Tmp
	add		%o7,%l4,%l4
	or		%i4,%i3,%i4
	or		%i5,%l1,%l1
	add		%i4,%l4,%l4
	 std		%f20,[%o5+40]		!
	sll		%l4,5,%i3			!! 46
	ld		[%o5+56],%o7
	 .word	0x9fb30daf !fxors	%f12,%f15,%f15! 0/ 0/ 0:X[1]^=X[14]
	srl		%l4,27,%i4
	add		%i3,%l3,%l3
	 .word	0xa5b00d86 !fxor	%f0,%f6,%f18! 1/ 1/ 1:Tmp=X[2,3]^X[8,9]
	and		%l1,%l0,%i3
	add		%i4,%l3,%l3
	sll		%l0,30,%i5
	or		%l1,%l0,%i4
	 .word	0x9db48d8e !fxor	%f18,%f14,%f14		! 2/ 4/ 3:X[0,1]^=X[2,3]^X[8,9]
	srl		%l0,2,%l0
	and		%l2,%i4,%i4
	add		%o7,%l3,%l3
	or		%i4,%i3,%i4
	 .word	0xa5b3890e !faligndata	%f14,%f14,%f18		! 3/ 7/ 5:Tmp=X[0,1]>>>24
	or		%i5,%l0,%l0
	add		%i4,%l3,%l3
	 .word	0x9db38a4e !fpadd32	%f14,%f14,%f14		! 4/ 8/ 6:X[0,1]<<=1
	sll		%l3,5,%i3			!! 47
	ld		[%o5+60],%o7
	srl		%l3,27,%i4
	add		%i3,%l2,%l2
	 .word	0xa5b486e1 !fmul8ulx16	%f18,%f32,%f18			! 5/10/ 7:Tmp>>=7, Tmp&=1
	and		%l0,%l4,%i3
	add		%i4,%l2,%l2
	 .word	0xa9b24a4c !fpadd32	%f40,%f12,%f20			!
	sll		%l4,30,%i5
	or		%l0,%l4,%i4
	 .word	0x81b34da0 !fxors	%f13,%f0,%f0	!-1/-1/-1:X[0]^=X[13]
	srl		%l4,2,%l4
	and		%l1,%i4,%i4
	 .word	0x9db48d8e !fxor	%f18,%f14,%f14		! 8/14/10:X[0,1]|=Tmp
	add		%o7,%l2,%l2
	or		%i4,%i3,%i4
	or		%i5,%l4,%l4
	add		%i4,%l2,%l2
	 std		%f20,[%o5+48]		!
	sll		%l2,5,%i3			!! 48
	ld		[%o5+0],%o7
	 .word	0x83b38da1 !fxors	%f14,%f1,%f1! 0/ 0/ 0:X[1]^=X[14]
	srl		%l2,27,%i4
	add		%i3,%l1,%l1
	 .word	0xa5b08d88 !fxor	%f2,%f8,%f18! 1/ 1/ 1:Tmp=X[2,3]^X[8,9]
	and		%l4,%l3,%i3
	add		%i4,%l1,%l1
	sll		%l3,30,%i5
	or		%l4,%l3,%i4
	 .word	0x81b48d80 !fxor	%f18,%f0,%f0		! 2/ 4/ 3:X[0,1]^=X[2,3]^X[8,9]
	srl		%l3,2,%l3
	and		%l0,%i4,%i4
	add		%o7,%l1,%l1
	or		%i4,%i3,%i4
	 .word	0xa5b00900 !faligndata	%f0,%f0,%f18		! 3/ 7/ 5:Tmp=X[0,1]>>>24
	or		%i5,%l3,%l3
	add		%i4,%l1,%l1
	 .word	0x81b00a40 !fpadd32	%f0,%f0,%f0		! 4/ 8/ 6:X[0,1]<<=1
	sll		%l1,5,%i3			!! 49
	ld		[%o5+4],%o7
	srl		%l1,27,%i4
	add		%i3,%l0,%l0
	 .word	0xa5b486e1 !fmul8ulx16	%f18,%f32,%f18			! 5/10/ 7:Tmp>>=7, Tmp&=1
	and		%l3,%l2,%i3
	add		%i4,%l0,%l0
	 .word	0xa9b24a4e !fpadd32	%f40,%f14,%f20			!
	sll		%l2,30,%i5
	or		%l3,%l2,%i4
	 .word	0x85b3cda2 !fxors	%f15,%f2,%f2	!-1/-1/-1:X[0]^=X[13]
	srl		%l2,2,%l2
	and		%l4,%i4,%i4
	 .word	0x81b48d80 !fxor	%f18,%f0,%f0		! 8/14/10:X[0,1]|=Tmp
	add		%o7,%l0,%l0
	or		%i4,%i3,%i4
	or		%i5,%l2,%l2
	add		%i4,%l0,%l0
	 std		%f20,[%o5+56]		!
	sll		%l0,5,%i3			!! 50
	ld		[%o5+8],%o7
	 .word	0x87b00da3 !fxors	%f0,%f3,%f3! 0/ 0/ 0:X[1]^=X[14]
	srl		%l0,27,%i4
	add		%i3,%l4,%l4
	 .word	0xa5b10d8a !fxor	%f4,%f10,%f18! 1/ 1/ 1:Tmp=X[2,3]^X[8,9]
	and		%l2,%l1,%i3
	add		%i4,%l4,%l4
	sll		%l1,30,%i5
	or		%l2,%l1,%i4
	 .word	0x85b48d82 !fxor	%f18,%f2,%f2		! 2/ 4/ 3:X[0,1]^=X[2,3]^X[8,9]
	srl		%l1,2,%l1
	and		%l3,%i4,%i4
	add		%o7,%l4,%l4
	or		%i4,%i3,%i4
	 .word	0xa5b08902 !faligndata	%f2,%f2,%f18		! 3/ 7/ 5:Tmp=X[0,1]>>>24
	or		%i5,%l1,%l1
	add		%i4,%l4,%l4
	 .word	0x85b08a42 !fpadd32	%f2,%f2,%f2		! 4/ 8/ 6:X[0,1]<<=1
	sll		%l4,5,%i3			!! 51
	ld		[%o5+12],%o7
	srl		%l4,27,%i4
	add		%i3,%l3,%l3
	 .word	0xa5b486e1 !fmul8ulx16	%f18,%f32,%f18			! 5/10/ 7:Tmp>>=7, Tmp&=1
	and		%l1,%l0,%i3
	add		%i4,%l3,%l3
	 .word	0xa9b24a40 !fpadd32	%f40,%f0,%f20			!
	sll		%l0,30,%i5
	or		%l1,%l0,%i4
	 .word	0x89b04da4 !fxors	%f1,%f4,%f4	!-1/-1/-1:X[0]^=X[13]
	srl		%l0,2,%l0
	and		%l2,%i4,%i4
	 .word	0x85b48d82 !fxor	%f18,%f2,%f2		! 8/14/10:X[0,1]|=Tmp
	add		%o7,%l3,%l3
	or		%i4,%i3,%i4
	or		%i5,%l0,%l0
	add		%i4,%l3,%l3
	 std		%f20,[%o5+0]		!
	sll		%l3,5,%i3			!! 52
	ld		[%o5+16],%o7
	 .word	0x8bb08da5 !fxors	%f2,%f5,%f5! 0/ 0/ 0:X[1]^=X[14]
	srl		%l3,27,%i4
	add		%i3,%l2,%l2
	 .word	0xa5b18d8c !fxor	%f6,%f12,%f18! 1/ 1/ 1:Tmp=X[2,3]^X[8,9]
	and		%l0,%l4,%i3
	add		%i4,%l2,%l2
	sll		%l4,30,%i5
	or		%l0,%l4,%i4
	 .word	0x89b48d84 !fxor	%f18,%f4,%f4		! 2/ 4/ 3:X[0,1]^=X[2,3]^X[8,9]
	srl		%l4,2,%l4
	and		%l1,%i4,%i4
	add		%o7,%l2,%l2
	or		%i4,%i3,%i4
	 .word	0xa5b10904 !faligndata	%f4,%f4,%f18		! 3/ 7/ 5:Tmp=X[0,1]>>>24
	or		%i5,%l4,%l4
	add		%i4,%l2,%l2
	 .word	0x89b10a44 !fpadd32	%f4,%f4,%f4		! 4/ 8/ 6:X[0,1]<<=1
	sll		%l2,5,%i3			!! 53
	ld		[%o5+20],%o7
	srl		%l2,27,%i4
	add		%i3,%l1,%l1
	 .word	0xa5b486e1 !fmul8ulx16	%f18,%f32,%f18			! 5/10/ 7:Tmp>>=7, Tmp&=1
	and		%l4,%l3,%i3
	add		%i4,%l1,%l1
	 .word	0xa9b24a42 !fpadd32	%f40,%f2,%f20			!
	sll		%l3,30,%i5
	or		%l4,%l3,%i4
	 .word	0x8db0cda6 !fxors	%f3,%f6,%f6	!-1/-1/-1:X[0]^=X[13]
	srl		%l3,2,%l3
	and		%l0,%i4,%i4
	 .word	0x89b48d84 !fxor	%f18,%f4,%f4		! 8/14/10:X[0,1]|=Tmp
	add		%o7,%l1,%l1
	or		%i4,%i3,%i4
	or		%i5,%l3,%l3
	add		%i4,%l1,%l1
	 std		%f20,[%o5+8]		!
	sll		%l1,5,%i3			!! 54
	ld		[%o5+24],%o7
	 .word	0x8fb10da7 !fxors	%f4,%f7,%f7! 0/ 0/ 0:X[1]^=X[14]
	srl		%l1,27,%i4
	add		%i3,%l0,%l0
	 .word	0xa5b20d8e !fxor	%f8,%f14,%f18! 1/ 1/ 1:Tmp=X[2,3]^X[8,9]
	and		%l3,%l2,%i3
	add		%i4,%l0,%l0
	sll		%l2,30,%i5
	or		%l3,%l2,%i4
	 .word	0x8db48d86 !fxor	%f18,%f6,%f6		! 2/ 4/ 3:X[0,1]^=X[2,3]^X[8,9]
	srl		%l2,2,%l2
	and		%l4,%i4,%i4
	add		%o7,%l0,%l0
	or		%i4,%i3,%i4
	 .word	0xa5b18906 !faligndata	%f6,%f6,%f18		! 3/ 7/ 5:Tmp=X[0,1]>>>24
	or		%i5,%l2,%l2
	add		%i4,%l0,%l0
	 .word	0x8db18a46 !fpadd32	%f6,%f6,%f6		! 4/ 8/ 6:X[0,1]<<=1
	sll		%l0,5,%i3			!! 55
	ld		[%o5+28],%o7
	srl		%l0,27,%i4
	add		%i3,%l4,%l4
	 .word	0xa5b486e1 !fmul8ulx16	%f18,%f32,%f18			! 5/10/ 7:Tmp>>=7, Tmp&=1
	and		%l2,%l1,%i3
	add		%i4,%l4,%l4
	 .word	0xa9b24a44 !fpadd32	%f40,%f4,%f20			!
	sll		%l1,30,%i5
	or		%l2,%l1,%i4
	 .word	0x91b14da8 !fxors	%f5,%f8,%f8	!-1/-1/-1:X[0]^=X[13]
	srl		%l1,2,%l1
	and		%l3,%i4,%i4
	 .word	0x8db48d86 !fxor	%f18,%f6,%f6		! 8/14/10:X[0,1]|=Tmp
	add		%o7,%l4,%l4
	or		%i4,%i3,%i4
	or		%i5,%l1,%l1
	add		%i4,%l4,%l4
	 std		%f20,[%o5+16]		!
	sll		%l4,5,%i3			!! 56
	ld		[%o5+32],%o7
	 .word	0x93b18da9 !fxors	%f6,%f9,%f9! 0/ 0/ 0:X[1]^=X[14]
	srl		%l4,27,%i4
	add		%i3,%l3,%l3
	 .word	0xa5b28d80 !fxor	%f10,%f0,%f18! 1/ 1/ 1:Tmp=X[2,3]^X[8,9]
	and		%l1,%l0,%i3
	add		%i4,%l3,%l3
	sll		%l0,30,%i5
	or		%l1,%l0,%i4
	 .word	0x91b48d88 !fxor	%f18,%f8,%f8		! 2/ 4/ 3:X[0,1]^=X[2,3]^X[8,9]
	srl		%l0,2,%l0
	and		%l2,%i4,%i4
	add		%o7,%l3,%l3
	or		%i4,%i3,%i4
	 .word	0xa5b20908 !faligndata	%f8,%f8,%f18		! 3/ 7/ 5:Tmp=X[0,1]>>>24
	or		%i5,%l0,%l0
	add		%i4,%l3,%l3
	 .word	0x91b20a48 !fpadd32	%f8,%f8,%f8		! 4/ 8/ 6:X[0,1]<<=1
	sll		%l3,5,%i3			!! 57
	ld		[%o5+36],%o7
	srl		%l3,27,%i4
	add		%i3,%l2,%l2
	 .word	0xa5b486e1 !fmul8ulx16	%f18,%f32,%f18			! 5/10/ 7:Tmp>>=7, Tmp&=1
	and		%l0,%l4,%i3
	add		%i4,%l2,%l2
	 .word	0xa9b24a46 !fpadd32	%f40,%f6,%f20			!
	sll		%l4,30,%i5
	or		%l0,%l4,%i4
	 .word	0x95b1cdaa !fxors	%f7,%f10,%f10	!-1/-1/-1:X[0]^=X[13]
	srl		%l4,2,%l4
	and		%l1,%i4,%i4
	 .word	0x91b48d88 !fxor	%f18,%f8,%f8		! 8/14/10:X[0,1]|=Tmp
	add		%o7,%l2,%l2
	or		%i4,%i3,%i4
	or		%i5,%l4,%l4
	add		%i4,%l2,%l2
	 std		%f20,[%o5+24]		!
	sll		%l2,5,%i3			!! 58
	ld		[%o5+40],%o7
	 .word	0x97b20dab !fxors	%f8,%f11,%f11! 0/ 0/ 0:X[1]^=X[14]
	srl		%l2,27,%i4
	add		%i3,%l1,%l1
	 .word	0xa5b30d82 !fxor	%f12,%f2,%f18! 1/ 1/ 1:Tmp=X[2,3]^X[8,9]
	and		%l4,%l3,%i3
	add		%i4,%l1,%l1
	sll		%l3,30,%i5
	or		%l4,%l3,%i4
	 .word	0x95b48d8a !fxor	%f18,%f10,%f10		! 2/ 4/ 3:X[0,1]^=X[2,3]^X[8,9]
	srl		%l3,2,%l3
	and		%l0,%i4,%i4
	add		%o7,%l1,%l1
	or		%i4,%i3,%i4
	 .word	0xa5b2890a !faligndata	%f10,%f10,%f18		! 3/ 7/ 5:Tmp=X[0,1]>>>24
	or		%i5,%l3,%l3
	add		%i4,%l1,%l1
	 .word	0x95b28a4a !fpadd32	%f10,%f10,%f10		! 4/ 8/ 6:X[0,1]<<=1
	sll		%l1,5,%i3			!! 59
	ld		[%o5+44],%o7
	srl		%l1,27,%i4
	add		%i3,%l0,%l0
	 .word	0xa5b486e1 !fmul8ulx16	%f18,%f32,%f18			! 5/10/ 7:Tmp>>=7, Tmp&=1
	and		%l3,%l2,%i3
	add		%i4,%l0,%l0
	 .word	0xa9b24a48 !fpadd32	%f40,%f8,%f20			!
	sll		%l2,30,%i5
	or		%l3,%l2,%i4
	 .word	0x99b24dac !fxors	%f9,%f12,%f12	!-1/-1/-1:X[0]^=X[13]
	srl		%l2,2,%l2
	and		%l4,%i4,%i4
	 .word	0x95b48d8a !fxor	%f18,%f10,%f10		! 8/14/10:X[0,1]|=Tmp
	add		%o7,%l0,%l0
	or		%i4,%i3,%i4
	or		%i5,%l2,%l2
	add		%i4,%l0,%l0
	 std		%f20,[%o5+32]		!
	sll		%l0,5,%i3			!! 60
	ld		[%o5+48],%o7
	 .word	0x9bb28dad !fxors	%f10,%f13,%f13! 0/ 0/ 0:X[1]^=X[14]
	srl		%l0,27,%i4
	add		%i3,%l4,%l4
	 .word	0xa5b38d84 !fxor	%f14,%f4,%f18! 1/ 1/ 1:Tmp=X[2,3]^X[8,9]
	xor		%l2,%l1,%i3
	add		%i4,%l4,%l4
	sll		%l1,30,%i5
	xor		%l3,%i3,%i4
	 .word	0x99b48d8c !fxor	%f18,%f12,%f12		! 2/ 4/ 3:X[0,1]^=X[2,3]^X[8,9]
	srl		%l1,2,%l1
	add		%i4,%l4,%l4
	or		%i5,%l1,%l1
	add		%o7,%l4,%l4
	 .word	0xa5b3090c !faligndata	%f12,%f12,%f18		! 3/ 7/ 5:Tmp=X[0,1]>>>24
	sll		%l4,5,%i3			!! 61
	ld		[%o5+52],%o7
	 .word	0x99b30a4c !fpadd32	%f12,%f12,%f12		! 4/ 8/ 6:X[0,1]<<=1
	srl		%l4,27,%i4
	add		%i3,%l3,%l3
	 .word	0xa5b486e1 !fmul8ulx16	%f18,%f32,%f18			! 5/10/ 7:Tmp>>=7, Tmp&=1
	xor		%l1,%l0,%i3
	add		%i4,%l3,%l3
	 .word	0xa9b24a4a !fpadd32	%f40,%f10,%f20			!
	sll		%l0,30,%i5
	xor		%l2,%i3,%i4
	 .word	0x9db2cdae !fxors	%f11,%f14,%f14	!-1/-1/-1:X[0]^=X[13]
	srl		%l0,2,%l0
	add		%i4,%l3,%l3
	 .word	0x99b48d8c !fxor	%f18,%f12,%f12		! 8/14/10:X[0,1]|=Tmp
	or		%i5,%l0,%l0
	add		%o7,%l3,%l3
	 std		%f20,[%o5+40]		!
	sll		%l3,5,%i3			!! 62
	ld		[%o5+56],%o7
	 .word	0x9fb30daf !fxors	%f12,%f15,%f15! 0/ 0/ 0:X[1]^=X[14]
	srl		%l3,27,%i4
	add		%i3,%l2,%l2
	 .word	0xa5b00d86 !fxor	%f0,%f6,%f18! 1/ 1/ 1:Tmp=X[2,3]^X[8,9]
	xor		%l0,%l4,%i3
	add		%i4,%l2,%l2
	sll		%l4,30,%i5
	xor		%l1,%i3,%i4
	 .word	0x9db48d8e !fxor	%f18,%f14,%f14		! 2/ 4/ 3:X[0,1]^=X[2,3]^X[8,9]
	srl		%l4,2,%l4
	add		%i4,%l2,%l2
	or		%i5,%l4,%l4
	add		%o7,%l2,%l2
	 .word	0xa5b3890e !faligndata	%f14,%f14,%f18		! 3/ 7/ 5:Tmp=X[0,1]>>>24
	sll		%l2,5,%i3			!! 63
	ld		[%o5+60],%o7
	 .word	0x9db38a4e !fpadd32	%f14,%f14,%f14		! 4/ 8/ 6:X[0,1]<<=1
	srl		%l2,27,%i4
	add		%i3,%l1,%l1
	 .word	0xa5b486e1 !fmul8ulx16	%f18,%f32,%f18			! 5/10/ 7:Tmp>>=7, Tmp&=1
	xor		%l4,%l3,%i3
	add		%i4,%l1,%l1
	 .word	0xa9b24a4c !fpadd32	%f40,%f12,%f20			!
	sll		%l3,30,%i5
	xor		%l0,%i3,%i4
	 .word	0x81b34da0 !fxors	%f13,%f0,%f0	!-1/-1/-1:X[0]^=X[13]
	srl		%l3,2,%l3
	add		%i4,%l1,%l1
	 .word	0x9db48d8e !fxor	%f18,%f14,%f14		! 8/14/10:X[0,1]|=Tmp
	or		%i5,%l3,%l3
	add		%o7,%l1,%l1
	 std		%f20,[%o5+48]		!
	sll		%l1,5,%i3			!! 64
	ld		[%o5+0],%o7
	 .word	0xa9b24a4e !fpadd32	%f40,%f14,%f20
	srl		%l1,27,%i4
	add		%i3,%l0,%l0
	xor		%l3,%l2,%i3
	add		%i4,%l0,%l0
	sll		%l2,30,%i5
	xor		%l4,%i3,%i4
	 std		%f20,[%o5+56]
	srl		%l2,2,%l2
	add		%i4,%l0,%l0
	or		%i5,%l2,%l2
	add		%o7,%l0,%l0
	sll		%l0,5,%i3			!! 65
	ld		[%o5+4],%o7
	srl		%l0,27,%i4
	add		%i3,%l4,%l4
	xor		%l2,%l1,%i3
	add		%i4,%l4,%l4
	sll		%l1,30,%i5
	xor		%l3,%i3,%i4
	srl		%l1,2,%l1
	add		%i4,%l4,%l4
	or		%i5,%l1,%l1
	add		%o7,%l4,%l4
	sll		%l4,5,%i3			!! 66
	ld		[%o5+8],%o7
	srl		%l4,27,%i4
	add		%i3,%l3,%l3
	xor		%l1,%l0,%i3
	add		%i4,%l3,%l3
	sll		%l0,30,%i5
	xor		%l2,%i3,%i4
	srl		%l0,2,%l0
	add		%i4,%l3,%l3
	or		%i5,%l0,%l0
	add		%o7,%l3,%l3
	sll		%l3,5,%i3			!! 67
	ld		[%o5+12],%o7
	srl		%l3,27,%i4
	add		%i3,%l2,%l2
	xor		%l0,%l4,%i3
	add		%i4,%l2,%l2
	sll		%l4,30,%i5
	xor		%l1,%i3,%i4
	srl		%l4,2,%l4
	add		%i4,%l2,%l2
	or		%i5,%l4,%l4
	add		%o7,%l2,%l2
	sll		%l2,5,%i3			!! 68
	ld		[%o5+16],%o7
	srl		%l2,27,%i4
	add		%i3,%l1,%l1
	xor		%l4,%l3,%i3
	add		%i4,%l1,%l1
	sll		%l3,30,%i5
	xor		%l0,%i3,%i4
	srl		%l3,2,%l3
	add		%i4,%l1,%l1
	or		%i5,%l3,%l3
	add		%o7,%l1,%l1
	sll		%l1,5,%i3			!! 69
	ld		[%o5+20],%o7
	srl		%l1,27,%i4
	add		%i3,%l0,%l0
	xor		%l3,%l2,%i3
	add		%i4,%l0,%l0
	sll		%l2,30,%i5
	xor		%l4,%i3,%i4
	srl		%l2,2,%l2
	add		%i4,%l0,%l0
	or		%i5,%l2,%l2
	add		%o7,%l0,%l0
	tst		%i2
	bz,pn		%xcc,.Ltail
	nop
	sll		%l0,5,%i3			!! 70
	ld		[%o5+24],%o7
	srl		%l0,27,%i4
	add		%i3,%l4,%l4
	 ldd		[%i1+64],%f0
	xor		%l2,%l1,%i3
	add		%i4,%l4,%l4
	sll		%l1,30,%i5
	xor		%l3,%i3,%i4
	srl		%l1,2,%l1
	add		%i4,%l4,%l4
	or		%i5,%l1,%l1
	add		%o7,%l4,%l4

	and		%i1,-64,%g5
	inc		64,%i1
	and		%g5,255,%g5
	.word	0x81b00304 !alignaddr	%g0,%g4,%g0
	add		%g1,%g5,%g5
	 ldd		[%i1+8],%f2
	sll		%l4,5,%i3			!! 71
	ld		[%o5+28],%o7
	srl		%l4,27,%i4
	add		%i3,%l3,%l3
	xor		%l1,%l0,%i3
	add		%i4,%l3,%l3
	sll		%l0,30,%i5
	xor		%l2,%i3,%i4
	srl		%l0,2,%l0
	add		%i4,%l3,%l3
	or		%i5,%l0,%l0
	add		%o7,%l3,%l3
	 ldd		[%i1+16],%f4
	 .word	0x81b00902 !faligndata	%f0,%f2,%f0
	sll		%l3,5,%i3			!! 72
	ld		[%o5+32],%o7
	srl		%l3,27,%i4
	add		%i3,%l2,%l2
	xor		%l0,%l4,%i3
	add		%i4,%l2,%l2
	 .word	0xa9b0ca40 !fpadd32	%f34,%f0,%f20
	sll		%l4,30,%i5
	xor		%l1,%i3,%i4
	srl		%l4,2,%l4
	add		%i4,%l2,%l2
	or		%i5,%l4,%l4
	add		%o7,%l2,%l2
	 ldd		[%i1+24],%f6
	 std		%f20,[%g5+0]
	 .word	0x85b08904 !faligndata	%f2,%f4,%f2
	sll		%l2,5,%i3			!! 73
	ld		[%o5+36],%o7
	srl		%l2,27,%i4
	add		%i3,%l1,%l1
	xor		%l4,%l3,%i3
	add		%i4,%l1,%l1
	 .word	0xa9b0ca42 !fpadd32	%f34,%f2,%f20
	sll		%l3,30,%i5
	xor		%l0,%i3,%i4
	srl		%l3,2,%l3
	add		%i4,%l1,%l1
	or		%i5,%l3,%l3
	add		%o7,%l1,%l1
	 ldd		[%i1+32],%f8
	 std		%f20,[%g5+8]
	 .word	0x89b10906 !faligndata	%f4,%f6,%f4
	sll		%l1,5,%i3			!! 74
	ld		[%o5+40],%o7
	srl		%l1,27,%i4
	add		%i3,%l0,%l0
	xor		%l3,%l2,%i3
	add		%i4,%l0,%l0
	 .word	0xa9b0ca44 !fpadd32	%f34,%f4,%f20
	sll		%l2,30,%i5
	xor		%l4,%i3,%i4
	srl		%l2,2,%l2
	add		%i4,%l0,%l0
	or		%i5,%l2,%l2
	add		%o7,%l0,%l0
	 ldd		[%i1+40],%f10
	 std		%f20,[%g5+16]
	 .word	0x8db18908 !faligndata	%f6,%f8,%f6
	sll		%l0,5,%i3			!! 75
	ld		[%o5+44],%o7
	srl		%l0,27,%i4
	add		%i3,%l4,%l4
	xor		%l2,%l1,%i3
	add		%i4,%l4,%l4
	 .word	0xa9b0ca46 !fpadd32	%f34,%f6,%f20
	sll		%l1,30,%i5
	xor		%l3,%i3,%i4
	srl		%l1,2,%l1
	add		%i4,%l4,%l4
	or		%i5,%l1,%l1
	add		%o7,%l4,%l4
	 ldd		[%i1+48],%f12
	 std		%f20,[%g5+24]
	 .word	0x91b2090a !faligndata	%f8,%f10,%f8
	sll		%l4,5,%i3			!! 76
	ld		[%o5+48],%o7
	srl		%l4,27,%i4
	add		%i3,%l3,%l3
	xor		%l1,%l0,%i3
	add		%i4,%l3,%l3
	 .word	0xa9b0ca48 !fpadd32	%f34,%f8,%f20
	sll		%l0,30,%i5
	xor		%l2,%i3,%i4
	srl		%l0,2,%l0
	add		%i4,%l3,%l3
	or		%i5,%l0,%l0
	add		%o7,%l3,%l3
	 ldd		[%i1+56],%f14
	 std		%f20,[%g5+32]
	 .word	0x95b2890c !faligndata	%f10,%f12,%f10
	sll		%l3,5,%i3			!! 77
	ld		[%o5+52],%o7
	srl		%l3,27,%i4
	add		%i3,%l2,%l2
	xor		%l0,%l4,%i3
	add		%i4,%l2,%l2
	 .word	0xa9b0ca4a !fpadd32	%f34,%f10,%f20
	sll		%l4,30,%i5
	xor		%l1,%i3,%i4
	srl		%l4,2,%l4
	add		%i4,%l2,%l2
	or		%i5,%l4,%l4
	add		%o7,%l2,%l2
	 add		%g4,63,%i3
	 and		%i3,-8,%i3
	 ldd		[%i1+%i3],%f16
	 std		%f20,[%g5+40]
	 .word	0x99b3090e !faligndata	%f12,%f14,%f12
	sll		%l2,5,%i3			!! 78
	ld		[%o5+56],%o7
	srl		%l2,27,%i4
	add		%i3,%l1,%l1
	xor		%l4,%l3,%i3
	add		%i4,%l1,%l1
	 .word	0xa9b0ca4c !fpadd32	%f34,%f12,%f20
	sll		%l3,30,%i5
	xor		%l0,%i3,%i4
	srl		%l3,2,%l3
	add		%i4,%l1,%l1
	or		%i5,%l3,%l3
	add		%o7,%l1,%l1
	 std		%f20,[%g5+48]
	 .word	0x9db38910 !faligndata	%f14,%f16,%f14
	sll		%l1,5,%i3			!! 79
	ld		[%o5+60],%o7
	srl		%l1,27,%i4
	add		%i3,%l0,%l0
	xor		%l3,%l2,%i3
	add		%i4,%l0,%l0
	 .word	0xa9b0ca4e !fpadd32	%f34,%f14,%f20
	sll		%l2,30,%i5
	xor		%l4,%i3,%i4
	srl		%l2,2,%l2
	add		%i4,%l0,%l0
	or		%i5,%l2,%l2
	add		%o7,%l0,%l0
	 std		%f20,[%g5+56]
	add		%l0,%o0,%o0
	add		%l1,%o1,%o1
	add		%l2,%o2,%o2
	add		%l3,%o3,%o3
	add		%l4,%o4,%o4
	mov		5,%i3
	.word	0x81b34da0 !fxors	%f13,%f0,%f0
	mov		%o0,%l0
	mov		%o1,%l1
	mov		%o2,%l2
	mov		%o3,%l3
	mov		%o4,%l4
	.word	0x81b0031b !alignaddr	%g0,%i3,%g0	
	dec		1,%i2
	ba		.Loop
	mov		%g5,%o5

.align	32
.Ltail:
	sll		%l0,5,%i3			!! 70
	ld		[%o5+24],%o7
	srl		%l0,27,%i4
	add		%i3,%l4,%l4
	xor		%l2,%l1,%i3
	add		%i4,%l4,%l4
	sll		%l1,30,%i5
	xor		%l3,%i3,%i4
	srl		%l1,2,%l1
	add		%i4,%l4,%l4
	or		%i5,%l1,%l1
	add		%o7,%l4,%l4
	sll		%l4,5,%i3			!! 71
	ld		[%o5+28],%o7
	srl		%l4,27,%i4
	add		%i3,%l3,%l3
	xor		%l1,%l0,%i3
	add		%i4,%l3,%l3
	sll		%l0,30,%i5
	xor		%l2,%i3,%i4
	srl		%l0,2,%l0
	add		%i4,%l3,%l3
	or		%i5,%l0,%l0
	add		%o7,%l3,%l3
	sll		%l3,5,%i3			!! 72
	ld		[%o5+32],%o7
	srl		%l3,27,%i4
	add		%i3,%l2,%l2
	xor		%l0,%l4,%i3
	add		%i4,%l2,%l2
	sll		%l4,30,%i5
	xor		%l1,%i3,%i4
	srl		%l4,2,%l4
	add		%i4,%l2,%l2
	or		%i5,%l4,%l4
	add		%o7,%l2,%l2
	sll		%l2,5,%i3			!! 73
	ld		[%o5+36],%o7
	srl		%l2,27,%i4
	add		%i3,%l1,%l1
	xor		%l4,%l3,%i3
	add		%i4,%l1,%l1
	sll		%l3,30,%i5
	xor		%l0,%i3,%i4
	srl		%l3,2,%l3
	add		%i4,%l1,%l1
	or		%i5,%l3,%l3
	add		%o7,%l1,%l1
	sll		%l1,5,%i3			!! 74
	ld		[%o5+40],%o7
	srl		%l1,27,%i4
	add		%i3,%l0,%l0
	xor		%l3,%l2,%i3
	add		%i4,%l0,%l0
	sll		%l2,30,%i5
	xor		%l4,%i3,%i4
	srl		%l2,2,%l2
	add		%i4,%l0,%l0
	or		%i5,%l2,%l2
	add		%o7,%l0,%l0
	sll		%l0,5,%i3			!! 75
	ld		[%o5+44],%o7
	srl		%l0,27,%i4
	add		%i3,%l4,%l4
	xor		%l2,%l1,%i3
	add		%i4,%l4,%l4
	sll		%l1,30,%i5
	xor		%l3,%i3,%i4
	srl		%l1,2,%l1
	add		%i4,%l4,%l4
	or		%i5,%l1,%l1
	add		%o7,%l4,%l4
	sll		%l4,5,%i3			!! 76
	ld		[%o5+48],%o7
	srl		%l4,27,%i4
	add		%i3,%l3,%l3
	xor		%l1,%l0,%i3
	add		%i4,%l3,%l3
	sll		%l0,30,%i5
	xor		%l2,%i3,%i4
	srl		%l0,2,%l0
	add		%i4,%l3,%l3
	or		%i5,%l0,%l0
	add		%o7,%l3,%l3
	sll		%l3,5,%i3			!! 77
	ld		[%o5+52],%o7
	srl		%l3,27,%i4
	add		%i3,%l2,%l2
	xor		%l0,%l4,%i3
	add		%i4,%l2,%l2
	sll		%l4,30,%i5
	xor		%l1,%i3,%i4
	srl		%l4,2,%l4
	add		%i4,%l2,%l2
	or		%i5,%l4,%l4
	add		%o7,%l2,%l2
	sll		%l2,5,%i3			!! 78
	ld		[%o5+56],%o7
	srl		%l2,27,%i4
	add		%i3,%l1,%l1
	xor		%l4,%l3,%i3
	add		%i4,%l1,%l1
	sll		%l3,30,%i5
	xor		%l0,%i3,%i4
	srl		%l3,2,%l3
	add		%i4,%l1,%l1
	or		%i5,%l3,%l3
	add		%o7,%l1,%l1
	sll		%l1,5,%i3			!! 79
	ld		[%o5+60],%o7
	srl		%l1,27,%i4
	add		%i3,%l0,%l0
	xor		%l3,%l2,%i3
	add		%i4,%l0,%l0
	sll		%l2,30,%i5
	xor		%l4,%i3,%i4
	srl		%l2,2,%l2
	add		%i4,%l0,%l0
	or		%i5,%l2,%l2
	add		%o7,%l0,%l0
	add	%l0,%o0,%o0
	add	%l1,%o1,%o1
	add	%l2,%o2,%o2
	add	%l3,%o3,%o3
	add	%l4,%o4,%o4

	st	%o0,[%i0+0]
	st	%o1,[%i0+4]
	st	%o2,[%i0+8]
	st	%o3,[%i0+12]
	st	%o4,[%i0+16]

	ret
	restore
.type	sha1_block_data_order,#function
.size	sha1_block_data_order,(.-sha1_block_data_order)
.asciz	"SHA1 block transform for SPARCv9a, CRYPTOGAMS by <appro@openssl.org>"
.align	4
