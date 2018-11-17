.section	".text",#alloc,#execinstr
.globl	bn_mul_mont_vis3
.align	32
bn_mul_mont_vis3:
	add	%sp,	0,	%g4	! real top of stack
	sll	%o5,	2,	%o5	! size in bytes
	add	%o5,	63,	%g5
	andn	%g5,	63,	%g5	! buffer size rounded up to 64 bytes
	add	%g5,	%g5,	%g1
	add	%g5,	%g1,	%g1	! 3*buffer size
	sub	%g4,	%g1,	%g1
	andn	%g1,	63,	%g1	! align at 64 byte
	sub	%g1,	112,	%g1	! new top of stack
	sub	%g1,	%g4,	%g1

	save	%sp,	%g1,	%sp
	ld	[%i4+0],	%l0	! pull n0[0..1] value
	add	%sp, 0+112, %l5
	ld	[%i4+4],	%l1
	add	%l5,	%g5,	%l7
	ld	[%i2+0],	%l2	! m0=bp[0]
	sllx	%l1,	32,	%g1
	ld	[%i2+4],	%l3
	or	%l0,	%g1,	%g1
	add	%i2,	8,	%i2

	ld	[%i1+0],	%l0	! ap[0]
	sllx	%l3,	32,	%g2
	ld	[%i1+4],	%l1
	or	%l2,	%g2,	%g2

	ld	[%i1+8],	%l2	! ap[1]
	sllx	%l1,	32,	%o2
	ld	[%i1+12],	%l3
	or	%l0,	%o2,	%o2
	add	%i1,	16,	%i1
	stx	%o2,	[%l7]		! converted ap[0]

	mulx	%o2,	%g2,	%g4	! ap[0]*bp[0]
	.word	0x8bb282c2 !umulxhi	%o2,%g2,%g5

	ld	[%i3+0],	%l0	! np[0]
	sllx	%l3,	32,	%o2
	ld	[%i3+4],	%l1
	or	%l2,	%o2,	%o2

	ld	[%i3+8],	%l2	! np[1]
	sllx	%l1,	32,	%o4
	ld	[%i3+12],	%l3
	or	%l0, %o4,	%o4
	add	%i3,	16,	%i3
	stx	%o4,	[%l7+8]	! converted np[0]

	mulx	%g4,	%g1,	%g3	! "tp[0]"*n0
	stx	%o2,	[%l7+16]	! converted ap[1]

	mulx	%o2,	%g2,	%o3	! ap[1]*bp[0]
	.word	0x95b282c2 !umulxhi	%o2,%g2,%o2	! ahi=aj

	mulx	%o4,	%g3,	%o0	! np[0]*m1
	.word	0x93b302c3 !umulxhi	%o4,%g3,%o1

	sllx	%l3,	32,	%o4
	or	%l2,	%o4,	%o4
	stx	%o4,	[%l7+24]	! converted np[1]
	add	%l7,	32,	%l7

	addcc	%g4,	%o0,	%o0
	.word	0x93b00229 !addxc	%g0,%o1,%o1

	mulx	%o4,	%g3,	%o5	! np[1]*m1
	.word	0x99b302c3 !umulxhi	%o4,%g3,%o4	! nhi=nj

	ba	.L1st
	sub	%i5,	24,	%l4	! cnt=num-3

.align	16
.L1st:
	ld	[%i1+0],	%l0	! ap[j]
	addcc	%o3,	%g5,	%g4
	ld	[%i1+4],	%l1
	.word	0x8bb28220 !addxc	%o2,%g0,%g5

	sllx	%l1,	32,	%o2
	add	%i1,	8,	%i1
	or	%l0,	%o2,	%o2
	stx	%o2,	[%l7]		! converted ap[j]

	ld	[%i3+0],	%l2	! np[j]
	addcc	%o5,	%o1,	%o0
	ld	[%i3+4],	%l3
	.word	0x93b30220 !addxc	%o4,%g0,%o1	! nhi=nj

	sllx	%l3,	32,	%o4
	add	%i3,	8,	%i3
	mulx	%o2,	%g2,	%o3	! ap[j]*bp[0]
	or	%l2,	%o4,	%o4
	.word	0x95b282c2 !umulxhi	%o2,%g2,%o2	! ahi=aj
	stx	%o4,	[%l7+8]	! converted np[j]
	add	%l7,	16,	%l7	! anp++

	mulx	%o4,	%g3,	%o5	! np[j]*m1
	addcc	%g4,	%o0,	%o0	! np[j]*m1+ap[j]*bp[0]
	.word	0x99b302c3 !umulxhi	%o4,%g3,%o4	! nhi=nj
	.word	0x93b00229 !addxc	%g0,%o1,%o1
	stx	%o0,	[%l5]		! tp[j-1]
	add	%l5,	8,	%l5	! tp++

	brnz,pt	%l4,	.L1st
	sub	%l4,	8,	%l4	! j--
!.L1st
	addcc	%o3,	%g5,	%g4
	.word	0x8bb28220 !addxc	%o2,%g0,%g5	! ahi=aj

	addcc	%o5,	%o1,	%o0
	.word	0x93b30220 !addxc	%o4,%g0,%o1
	addcc	%g4,	%o0,	%o0	! np[j]*m1+ap[j]*bp[0]
	.word	0x93b00229 !addxc	%g0,%o1,%o1
	stx	%o0,	[%l5]		! tp[j-1]
	add	%l5,	8,	%l5

	addcc	%g5,	%o1,	%o1
	.word	0xa1b00220 !addxc	%g0,%g0,%l0	! upmost overflow bit
	stx	%o1,	[%l5]
	add	%l5,	8,	%l5

	ba	.Louter
	sub	%i5,	16,	%l1	! i=num-2

.align	16
.Louter:
	ld	[%i2+0],	%l2	! m0=bp[i]
	ld	[%i2+4],	%l3

	sub	%l7,	%i5,	%l7	! rewind
	sub	%l5,	%i5,	%l5
	sub	%l7,	%i5,	%l7

	add	%i2,	8,	%i2
	sllx	%l3,	32,	%g2
	ldx	[%l7+0],	%o2	! ap[0]
	or	%l2,	%g2,	%g2
	ldx	[%l7+8],	%o4	! np[0]

	mulx	%o2,	%g2,	%g4	! ap[0]*bp[i]
	ldx	[%l5],		%o7	! tp[0]
	.word	0x8bb282c2 !umulxhi	%o2,%g2,%g5
	ldx	[%l7+16],	%o2	! ap[1]
	addcc	%g4,	%o7,	%g4	! ap[0]*bp[i]+tp[0]
	mulx	%o2,	%g2,	%o3	! ap[1]*bp[i]
	.word	0x8bb00225 !addxc	%g0,%g5,%g5
	mulx	%g4,	%g1,	%g3	! tp[0]*n0
	.word	0x95b282c2 !umulxhi	%o2,%g2,%o2	! ahi=aj
	mulx	%o4,	%g3,	%o0	! np[0]*m1
	.word	0x93b302c3 !umulxhi	%o4,%g3,%o1
	ldx	[%l7+24],	%o4	! np[1]
	add	%l7,	32,	%l7
	addcc	%o0,	%g4,	%o0
	mulx	%o4,	%g3,	%o5	! np[1]*m1
	.word	0x93b00229 !addxc	%g0,%o1,%o1
	.word	0x99b302c3 !umulxhi	%o4,%g3,%o4	! nhi=nj

	ba	.Linner
	sub	%i5,	24,	%l4	! cnt=num-3
.align	16
.Linner:
	addcc	%o3,	%g5,	%g4
	ldx	[%l5+8],	%o7	! tp[j]
	.word	0x8bb28220 !addxc	%o2,%g0,%g5	! ahi=aj
	ldx	[%l7+0],	%o2	! ap[j]
	addcc	%o5,	%o1,	%o0
	mulx	%o2,	%g2,	%o3	! ap[j]*bp[i]
	.word	0x93b30220 !addxc	%o4,%g0,%o1	! nhi=nj
	ldx	[%l7+8],	%o4	! np[j]
	add	%l7,	16,	%l7
	.word	0x95b282c2 !umulxhi	%o2,%g2,%o2	! ahi=aj
	addcc	%g4,	%o7,	%g4	! ap[j]*bp[i]+tp[j]
	mulx	%o4,	%g3,	%o5	! np[j]*m1
	.word	0x8bb00225 !addxc	%g0,%g5,%g5
	.word	0x99b302c3 !umulxhi	%o4,%g3,%o4	! nhi=nj
	addcc	%o0,	%g4,	%o0	! np[j]*m1+ap[j]*bp[i]+tp[j]
	.word	0x93b00229 !addxc	%g0,%o1,%o1
	stx	%o0,	[%l5]		! tp[j-1]
	add	%l5,	8,	%l5
	brnz,pt	%l4,	.Linner
	sub	%l4,	8,	%l4
!.Linner
	ldx	[%l5+8],	%o7	! tp[j]
	addcc	%o3,	%g5,	%g4
	.word	0x8bb28220 !addxc	%o2,%g0,%g5	! ahi=aj
	addcc	%g4,	%o7,	%g4	! ap[j]*bp[i]+tp[j]
	.word	0x8bb00225 !addxc	%g0,%g5,%g5

	addcc	%o5,	%o1,	%o0
	.word	0x93b30220 !addxc	%o4,%g0,%o1	! nhi=nj
	addcc	%o0,	%g4,	%o0	! np[j]*m1+ap[j]*bp[i]+tp[j]
	.word	0x93b00229 !addxc	%g0,%o1,%o1
	stx	%o0,	[%l5]		! tp[j-1]

	subcc	%g0,	%l0,	%g0	! move upmost overflow to CCR.xcc
	.word	0x93b24265 !addxccc	%o1,%g5,%o1
	.word	0xa1b00220 !addxc	%g0,%g0,%l0
	stx	%o1,	[%l5+8]
	add	%l5,	16,	%l5

	brnz,pt	%l1,	.Louter
	sub	%l1,	8,	%l1

	sub	%l7,	%i5,	%l7	! rewind
	sub	%l5,	%i5,	%l5
	sub	%l7,	%i5,	%l7
	ba	.Lsub
	subcc	%i5,	8,	%l4	! cnt=num-1 and clear CCR.xcc

.align	16
.Lsub:
	ldx	[%l5],		%o7
	add	%l5,	8,	%l5
	ldx	[%l7+8],	%o4
	add	%l7,	16,	%l7
	subccc	%o7,	%o4,	%l2	! tp[j]-np[j]
	srlx	%o7,	32,	%o7
	srlx	%o4,	32,	%o4
	subccc	%o7,	%o4,	%l3
	add	%i0,	8,	%i0
	st	%l2,	[%i0-4]		! reverse order
	st	%l3,	[%i0-8]
	brnz,pt	%l4,	.Lsub
	sub	%l4,	8,	%l4

	sub	%l7,	%i5,	%l7	! rewind
	sub	%l5,	%i5,	%l5
	sub	%l7,	%i5,	%l7
	sub	%i0,	%i5,	%i0

	subc	%l0,	%g0,	%l0	! handle upmost overflow bit
	and	%l5,	%l0,	%i1
	andn	%i0,	%l0,	%i3
	or	%i3,	%i1,	%i1	! ap=borrow?tp:rp
	ba	.Lcopy
	sub	%i5,	8,	%l4

.align	16
.Lcopy:					! copy or in-place refresh
	ld	[%i1+0],	%l2
	ld	[%i1+4],	%l3
	add	%i1,	8,	%i1
	stx	%g0,	[%l5]		! zap
	add	%l5,	8,	%l5
	stx	%g0,	[%l7]		! zap
	stx	%g0,	[%l7+8]
	add	%l7,	16,	%l7
	st	%l3,	[%i0+0]		! flip order
	st	%l2,	[%i0+4]
	add	%i0,	8,	%i0
	brnz	%l4,	.Lcopy
	sub	%l4,	8,	%l4

	mov	1,	%o0
	ret
	restore
.type	bn_mul_mont_vis3, #function
.size	bn_mul_mont_vis3, .-bn_mul_mont_vis3
.asciz  "Montgomery Multiplication for SPARCv9 VIS3, CRYPTOGAMS by <appro@openssl.org>"
.align	4
