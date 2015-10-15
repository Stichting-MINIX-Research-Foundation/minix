.section	".text",#alloc,#execinstr

.global bn_mul_mont_fpu
.align  32
bn_mul_mont_fpu:
	save	%sp,-128-64,%sp

	cmp	%i5,4
	bl,a,pn %icc,.Lret
	clr	%i0
	andcc	%i5,1,%g0		! %i5 has to be even...
	bnz,a,pn %icc,.Lret
	clr	%i0			! signal "unsupported input value"

	srl	%i5,1,%i5
	sethi	%hi(0xffff),%l7
	ld	[%i4+0],%g4		! %g4 reassigned, remember?
	or	%l7,%lo(0xffff),%l7
	ld	[%i4+4],%o0
	sllx	%o0,32,%o0
	or	%o0,%g4,%g4		! %g4=n0[1].n0[0]

	sll	%i5,3,%i5		! num*=8

	add	%sp,0,%o0		! real top of stack
	sll	%i5,2,%o1
	add	%o1,%i5,%o1		! %o1=num*5
	sub	%o0,%o1,%o0
	and	%o0,-2048,%o0		! optimize TLB utilization
	sub	%o0,0,%sp		! alloca(5*num*8)

	rd	%asi,%o7		! save %asi
	add	%sp,0+128+64,%l0
	add	%l0,%i5,%l1
	add	%l1,%i5,%l1	! [an]p_[lh] point at the vectors' ends !
	add	%l1,%i5,%l2
	add	%l2,%i5,%l3
	add	%l3,%i5,%l4

	wr	%g0,210,%asi	! setup %asi for 16-bit FP loads

	add	%i0,%i5,%i0		! readjust input pointers to point
	add	%i1,%i5,%i1		! at the ends too...
	add	%i2,%i5,%i2
	add	%i3,%i5,%i3

	stx	%o7,[%sp+0+128+48]	! save %asi

	sub	%g0,%i5,%l5		! i=-num
	sub	%g0,%i5,%l6		! j=-num

	add	%i1,%l6,%o3
	add	%i2,%l5,%o4

	ld	[%o3+4],%g1		! bp[0]
	ld	[%o3+0],%o0
	ld	[%o4+4],%g5		! ap[0]
	sllx	%g1,32,%g1
	ld	[%o4+0],%o1
	sllx	%g5,32,%g5
	or	%g1,%o0,%o0
	or	%g5,%o1,%o1

	add	%i3,%l6,%o5

	mulx	%o1,%o0,%o0		! ap[0]*bp[0]
	mulx	%g4,%o0,%o0		! ap[0]*bp[0]*n0
	stx	%o0,[%sp+0+128+0]

	ld	[%o3+0],%f17	! load a[j] as pair of 32-bit words
	.word	0xa1b00c20	! fzeros %f16
	ld	[%o3+4],%f19
	.word	0xa5b00c20	! fzeros %f18
	ld	[%o5+0],%f21	! load n[j] as pair of 32-bit words
	.word	0xa9b00c20	! fzeros %f20
	ld	[%o5+4],%f23
	.word	0xadb00c20	! fzeros %f22

	! transfer b[i] to FPU as 4x16-bit values
	ldda	[%o4+2]%asi,%f0
	fxtod	%f16,%f16
	ldda	[%o4+0]%asi,%f2
	fxtod	%f18,%f18
	ldda	[%o4+6]%asi,%f4
	fxtod	%f20,%f20
	ldda	[%o4+4]%asi,%f6
	fxtod	%f22,%f22

	! transfer ap[0]*b[0]*n0 to FPU as 4x16-bit values
	ldda	[%sp+0+128+6]%asi,%f8
	fxtod	%f0,%f0
	ldda	[%sp+0+128+4]%asi,%f10
	fxtod	%f2,%f2
	ldda	[%sp+0+128+2]%asi,%f12
	fxtod	%f4,%f4
	ldda	[%sp+0+128+0]%asi,%f14
	fxtod	%f6,%f6

	std	%f16,[%l1+%l6]		! save smashed ap[j] in double format
	fxtod	%f8,%f8
	std	%f18,[%l2+%l6]
	fxtod	%f10,%f10
	std	%f20,[%l3+%l6]		! save smashed np[j] in double format
	fxtod	%f12,%f12
	std	%f22,[%l4+%l6]
	fxtod	%f14,%f14

		fmuld	%f16,%f0,%f32
		fmuld	%f20,%f8,%f48
		fmuld	%f16,%f2,%f34
		fmuld	%f20,%f10,%f50
		fmuld	%f16,%f4,%f36
	faddd	%f32,%f48,%f48
		fmuld	%f20,%f12,%f52
		fmuld	%f16,%f6,%f38
	faddd	%f34,%f50,%f50
		fmuld	%f20,%f14,%f54
		fmuld	%f18,%f0,%f40
	faddd	%f36,%f52,%f52
		fmuld	%f22,%f8,%f56
		fmuld	%f18,%f2,%f42
	faddd	%f38,%f54,%f54
		fmuld	%f22,%f10,%f58
		fmuld	%f18,%f4,%f44
	faddd	%f40,%f56,%f56
		fmuld	%f22,%f12,%f60
		fmuld	%f18,%f6,%f46
	faddd	%f42,%f58,%f58
		fmuld	%f22,%f14,%f62

	faddd	%f44,%f60,%f24	! %f60
	faddd	%f46,%f62,%f26	! %f62

	faddd	%f52,%f56,%f52
	faddd	%f54,%f58,%f54

	fdtox	%f48,%f48
	fdtox	%f50,%f50
	fdtox	%f52,%f52
	fdtox	%f54,%f54

	std	%f48,[%sp+0+128+0]
	add	%l6,8,%l6
	std	%f50,[%sp+0+128+8]
	add	%i1,%l6,%o4
	std	%f52,[%sp+0+128+16]
	add	%i3,%l6,%o5
	std	%f54,[%sp+0+128+24]

	ld	[%o4+0],%f17	! load a[j] as pair of 32-bit words
	.word	0xa1b00c20	! fzeros %f16
	ld	[%o4+4],%f19
	.word	0xa5b00c20	! fzeros %f18
	ld	[%o5+0],%f21	! load n[j] as pair of 32-bit words
	.word	0xa9b00c20	! fzeros %f20
	ld	[%o5+4],%f23
	.word	0xadb00c20	! fzeros %f22

	fxtod	%f16,%f16
	fxtod	%f18,%f18
	fxtod	%f20,%f20
	fxtod	%f22,%f22

	ldx	[%sp+0+128+0],%o0
		fmuld	%f16,%f0,%f32
	ldx	[%sp+0+128+8],%o1
		fmuld	%f20,%f8,%f48
	ldx	[%sp+0+128+16],%o2
		fmuld	%f16,%f2,%f34
	ldx	[%sp+0+128+24],%o3
		fmuld	%f20,%f10,%f50

	srlx	%o0,16,%o7
	std	%f16,[%l1+%l6]		! save smashed ap[j] in double format
		fmuld	%f16,%f4,%f36
	add	%o7,%o1,%o1
	std	%f18,[%l2+%l6]
		faddd	%f32,%f48,%f48
		fmuld	%f20,%f12,%f52
	srlx	%o1,16,%o7
	std	%f20,[%l3+%l6]		! save smashed np[j] in double format
		fmuld	%f16,%f6,%f38
	add	%o7,%o2,%o2
	std	%f22,[%l4+%l6]
		faddd	%f34,%f50,%f50
		fmuld	%f20,%f14,%f54
	srlx	%o2,16,%o7
		fmuld	%f18,%f0,%f40
	add	%o7,%o3,%o3		! %o3.%o2[0..15].%o1[0..15].%o0[0..15]
		faddd	%f36,%f52,%f52
		fmuld	%f22,%f8,%f56
	!and	%o0,%l7,%o0
	!and	%o1,%l7,%o1
	!and	%o2,%l7,%o2
	!sllx	%o1,16,%o1
	!sllx	%o2,32,%o2
	!sllx	%o3,48,%o7
	!or	%o1,%o0,%o0
	!or	%o2,%o0,%o0
	!or	%o7,%o0,%o0		! 64-bit result
	srlx	%o3,16,%g1		! 34-bit carry
		fmuld	%f18,%f2,%f42

	faddd	%f38,%f54,%f54
		fmuld	%f22,%f10,%f58
		fmuld	%f18,%f4,%f44
	faddd	%f40,%f56,%f56
		fmuld	%f22,%f12,%f60
		fmuld	%f18,%f6,%f46
	faddd	%f42,%f58,%f58
		fmuld	%f22,%f14,%f62

	faddd	%f24,%f48,%f48
	faddd	%f26,%f50,%f50
	faddd	%f44,%f60,%f24	! %f60
	faddd	%f46,%f62,%f26	! %f62

	faddd	%f52,%f56,%f52
	faddd	%f54,%f58,%f54

	fdtox	%f48,%f48
	fdtox	%f50,%f50
	fdtox	%f52,%f52
	fdtox	%f54,%f54

	std	%f48,[%sp+0+128+0]
	std	%f50,[%sp+0+128+8]
	addcc	%l6,8,%l6
	std	%f52,[%sp+0+128+16]
	bz,pn	%icc,.L1stskip
	std	%f54,[%sp+0+128+24]

.align	32			! incidentally already aligned !
.L1st:
	add	%i1,%l6,%o4
	add	%i3,%l6,%o5
	ld	[%o4+0],%f17	! load a[j] as pair of 32-bit words
	.word	0xa1b00c20	! fzeros %f16
	ld	[%o4+4],%f19
	.word	0xa5b00c20	! fzeros %f18
	ld	[%o5+0],%f21	! load n[j] as pair of 32-bit words
	.word	0xa9b00c20	! fzeros %f20
	ld	[%o5+4],%f23
	.word	0xadb00c20	! fzeros %f22

	fxtod	%f16,%f16
	fxtod	%f18,%f18
	fxtod	%f20,%f20
	fxtod	%f22,%f22

	ldx	[%sp+0+128+0],%o0
		fmuld	%f16,%f0,%f32
	ldx	[%sp+0+128+8],%o1
		fmuld	%f20,%f8,%f48
	ldx	[%sp+0+128+16],%o2
		fmuld	%f16,%f2,%f34
	ldx	[%sp+0+128+24],%o3
		fmuld	%f20,%f10,%f50

	srlx	%o0,16,%o7
	std	%f16,[%l1+%l6]		! save smashed ap[j] in double format
		fmuld	%f16,%f4,%f36
	add	%o7,%o1,%o1
	std	%f18,[%l2+%l6]
		faddd	%f32,%f48,%f48
		fmuld	%f20,%f12,%f52
	srlx	%o1,16,%o7
	std	%f20,[%l3+%l6]		! save smashed np[j] in double format
		fmuld	%f16,%f6,%f38
	add	%o7,%o2,%o2
	std	%f22,[%l4+%l6]
		faddd	%f34,%f50,%f50
		fmuld	%f20,%f14,%f54
	srlx	%o2,16,%o7
		fmuld	%f18,%f0,%f40
	add	%o7,%o3,%o3		! %o3.%o2[0..15].%o1[0..15].%o0[0..15]
	and	%o0,%l7,%o0
		faddd	%f36,%f52,%f52
		fmuld	%f22,%f8,%f56
	and	%o1,%l7,%o1
	and	%o2,%l7,%o2
		fmuld	%f18,%f2,%f42
	sllx	%o1,16,%o1
		faddd	%f38,%f54,%f54
		fmuld	%f22,%f10,%f58
	sllx	%o2,32,%o2
		fmuld	%f18,%f4,%f44
	sllx	%o3,48,%o7
	or	%o1,%o0,%o0
		faddd	%f40,%f56,%f56
		fmuld	%f22,%f12,%f60
	or	%o2,%o0,%o0
		fmuld	%f18,%f6,%f46
	or	%o7,%o0,%o0		! 64-bit result
		faddd	%f42,%f58,%f58
		fmuld	%f22,%f14,%f62
	addcc	%g1,%o0,%o0
		faddd	%f24,%f48,%f48
	srlx	%o3,16,%g1		! 34-bit carry
		faddd	%f26,%f50,%f50
	bcs,a	%xcc,.+8
	add	%g1,1,%g1

	stx	%o0,[%l0]		! tp[j-1]=

	faddd	%f44,%f60,%f24	! %f60
	faddd	%f46,%f62,%f26	! %f62

	faddd	%f52,%f56,%f52
	faddd	%f54,%f58,%f54

	fdtox	%f48,%f48
	fdtox	%f50,%f50
	fdtox	%f52,%f52
	fdtox	%f54,%f54

	std	%f48,[%sp+0+128+0]
	std	%f50,[%sp+0+128+8]
	std	%f52,[%sp+0+128+16]
	std	%f54,[%sp+0+128+24]

	addcc	%l6,8,%l6
	bnz,pt	%icc,.L1st
	add	%l0,8,%l0

.L1stskip:
	fdtox	%f24,%f24
	fdtox	%f26,%f26

	ldx	[%sp+0+128+0],%o0
	ldx	[%sp+0+128+8],%o1
	ldx	[%sp+0+128+16],%o2
	ldx	[%sp+0+128+24],%o3

	srlx	%o0,16,%o7
	std	%f24,[%sp+0+128+32]
	add	%o7,%o1,%o1
	std	%f26,[%sp+0+128+40]
	srlx	%o1,16,%o7
	add	%o7,%o2,%o2
	srlx	%o2,16,%o7
	add	%o7,%o3,%o3		! %o3.%o2[0..15].%o1[0..15].%o0[0..15]
	and	%o0,%l7,%o0
	and	%o1,%l7,%o1
	and	%o2,%l7,%o2
	sllx	%o1,16,%o1
	sllx	%o2,32,%o2
	sllx	%o3,48,%o7
	or	%o1,%o0,%o0
	or	%o2,%o0,%o0
	or	%o7,%o0,%o0		! 64-bit result
	ldx	[%sp+0+128+32],%o4
	addcc	%g1,%o0,%o0
	ldx	[%sp+0+128+40],%o5
	srlx	%o3,16,%g1		! 34-bit carry
	bcs,a	%xcc,.+8
	add	%g1,1,%g1

	stx	%o0,[%l0]		! tp[j-1]=
	add	%l0,8,%l0

	srlx	%o4,16,%o7
	add	%o7,%o5,%o5
	and	%o4,%l7,%o4
	sllx	%o5,16,%o7
	or	%o7,%o4,%o4
	addcc	%g1,%o4,%o4
	srlx	%o5,48,%g1
	bcs,a	%xcc,.+8
	add	%g1,1,%g1

	mov	%g1,%i4
	stx	%o4,[%l0]		! tp[num-1]=

	ba	.Louter
	add	%l5,8,%l5
.align	32
.Louter:
	sub	%g0,%i5,%l6		! j=-num
	add	%sp,0+128+64,%l0

	add	%i1,%l6,%o3
	add	%i2,%l5,%o4

	ld	[%o3+4],%g1		! bp[i]
	ld	[%o3+0],%o0
	ld	[%o4+4],%g5		! ap[0]
	sllx	%g1,32,%g1
	ld	[%o4+0],%o1
	sllx	%g5,32,%g5
	or	%g1,%o0,%o0
	or	%g5,%o1,%o1

	ldx	[%l0],%o2		! tp[0]
	mulx	%o1,%o0,%o0
	addcc	%o2,%o0,%o0
	mulx	%g4,%o0,%o0		! (ap[0]*bp[i]+t[0])*n0
	stx	%o0,[%sp+0+128+0]

	! transfer b[i] to FPU as 4x16-bit values
	ldda	[%o4+2]%asi,%f0
	ldda	[%o4+0]%asi,%f2
	ldda	[%o4+6]%asi,%f4
	ldda	[%o4+4]%asi,%f6

	! transfer (ap[0]*b[i]+t[0])*n0 to FPU as 4x16-bit values
	ldda	[%sp+0+128+6]%asi,%f8
	fxtod	%f0,%f0
	ldda	[%sp+0+128+4]%asi,%f10
	fxtod	%f2,%f2
	ldda	[%sp+0+128+2]%asi,%f12
	fxtod	%f4,%f4
	ldda	[%sp+0+128+0]%asi,%f14
	fxtod	%f6,%f6
	ldd	[%l1+%l6],%f16		! load a[j] in double format
	fxtod	%f8,%f8
	ldd	[%l2+%l6],%f18
	fxtod	%f10,%f10
	ldd	[%l3+%l6],%f20		! load n[j] in double format
	fxtod	%f12,%f12
	ldd	[%l4+%l6],%f22
	fxtod	%f14,%f14

		fmuld	%f16,%f0,%f32
		fmuld	%f20,%f8,%f48
		fmuld	%f16,%f2,%f34
		fmuld	%f20,%f10,%f50
		fmuld	%f16,%f4,%f36
	faddd	%f32,%f48,%f48
		fmuld	%f20,%f12,%f52
		fmuld	%f16,%f6,%f38
	faddd	%f34,%f50,%f50
		fmuld	%f20,%f14,%f54
		fmuld	%f18,%f0,%f40
	faddd	%f36,%f52,%f52
		fmuld	%f22,%f8,%f56
		fmuld	%f18,%f2,%f42
	faddd	%f38,%f54,%f54
		fmuld	%f22,%f10,%f58
		fmuld	%f18,%f4,%f44
	faddd	%f40,%f56,%f56
		fmuld	%f22,%f12,%f60
		fmuld	%f18,%f6,%f46
	faddd	%f42,%f58,%f58
		fmuld	%f22,%f14,%f62

	faddd	%f44,%f60,%f24	! %f60
	faddd	%f46,%f62,%f26	! %f62

	faddd	%f52,%f56,%f52
	faddd	%f54,%f58,%f54

	fdtox	%f48,%f48
	fdtox	%f50,%f50
	fdtox	%f52,%f52
	fdtox	%f54,%f54

	std	%f48,[%sp+0+128+0]
	std	%f50,[%sp+0+128+8]
	std	%f52,[%sp+0+128+16]
	add	%l6,8,%l6
	std	%f54,[%sp+0+128+24]

	ldd	[%l1+%l6],%f16		! load a[j] in double format
	ldd	[%l2+%l6],%f18
	ldd	[%l3+%l6],%f20		! load n[j] in double format
	ldd	[%l4+%l6],%f22

		fmuld	%f16,%f0,%f32
		fmuld	%f20,%f8,%f48
		fmuld	%f16,%f2,%f34
		fmuld	%f20,%f10,%f50
		fmuld	%f16,%f4,%f36
	ldx	[%sp+0+128+0],%o0
		faddd	%f32,%f48,%f48
		fmuld	%f20,%f12,%f52
	ldx	[%sp+0+128+8],%o1
		fmuld	%f16,%f6,%f38
	ldx	[%sp+0+128+16],%o2
		faddd	%f34,%f50,%f50
		fmuld	%f20,%f14,%f54
	ldx	[%sp+0+128+24],%o3
		fmuld	%f18,%f0,%f40

	srlx	%o0,16,%o7
		faddd	%f36,%f52,%f52
		fmuld	%f22,%f8,%f56
	add	%o7,%o1,%o1
		fmuld	%f18,%f2,%f42
	srlx	%o1,16,%o7
		faddd	%f38,%f54,%f54
		fmuld	%f22,%f10,%f58
	add	%o7,%o2,%o2
		fmuld	%f18,%f4,%f44
	srlx	%o2,16,%o7
		faddd	%f40,%f56,%f56
		fmuld	%f22,%f12,%f60
	add	%o7,%o3,%o3		! %o3.%o2[0..15].%o1[0..15].%o0[0..15]
	! why?
	and	%o0,%l7,%o0
		fmuld	%f18,%f6,%f46
	and	%o1,%l7,%o1
	and	%o2,%l7,%o2
		faddd	%f42,%f58,%f58
		fmuld	%f22,%f14,%f62
	sllx	%o1,16,%o1
		faddd	%f24,%f48,%f48
	sllx	%o2,32,%o2
		faddd	%f26,%f50,%f50
	sllx	%o3,48,%o7
	or	%o1,%o0,%o0
		faddd	%f44,%f60,%f24	! %f60
	or	%o2,%o0,%o0
		faddd	%f46,%f62,%f26	! %f62
	or	%o7,%o0,%o0		! 64-bit result
	ldx	[%l0],%o7
		faddd	%f52,%f56,%f52
	addcc	%o7,%o0,%o0
	! end-of-why?
		faddd	%f54,%f58,%f54
	srlx	%o3,16,%g1		! 34-bit carry
		fdtox	%f48,%f48
	bcs,a	%xcc,.+8
	add	%g1,1,%g1

	fdtox	%f50,%f50
	fdtox	%f52,%f52
	fdtox	%f54,%f54

	std	%f48,[%sp+0+128+0]
	std	%f50,[%sp+0+128+8]
	addcc	%l6,8,%l6
	std	%f52,[%sp+0+128+16]
	bz,pn	%icc,.Linnerskip
	std	%f54,[%sp+0+128+24]

	ba	.Linner
	nop
.align	32
.Linner:
	ldd	[%l1+%l6],%f16		! load a[j] in double format
	ldd	[%l2+%l6],%f18
	ldd	[%l3+%l6],%f20		! load n[j] in double format
	ldd	[%l4+%l6],%f22

		fmuld	%f16,%f0,%f32
		fmuld	%f20,%f8,%f48
		fmuld	%f16,%f2,%f34
		fmuld	%f20,%f10,%f50
		fmuld	%f16,%f4,%f36
	ldx	[%sp+0+128+0],%o0
		faddd	%f32,%f48,%f48
		fmuld	%f20,%f12,%f52
	ldx	[%sp+0+128+8],%o1
		fmuld	%f16,%f6,%f38
	ldx	[%sp+0+128+16],%o2
		faddd	%f34,%f50,%f50
		fmuld	%f20,%f14,%f54
	ldx	[%sp+0+128+24],%o3
		fmuld	%f18,%f0,%f40

	srlx	%o0,16,%o7
		faddd	%f36,%f52,%f52
		fmuld	%f22,%f8,%f56
	add	%o7,%o1,%o1
		fmuld	%f18,%f2,%f42
	srlx	%o1,16,%o7
		faddd	%f38,%f54,%f54
		fmuld	%f22,%f10,%f58
	add	%o7,%o2,%o2
		fmuld	%f18,%f4,%f44
	srlx	%o2,16,%o7
		faddd	%f40,%f56,%f56
		fmuld	%f22,%f12,%f60
	add	%o7,%o3,%o3		! %o3.%o2[0..15].%o1[0..15].%o0[0..15]
	and	%o0,%l7,%o0
		fmuld	%f18,%f6,%f46
	and	%o1,%l7,%o1
	and	%o2,%l7,%o2
		faddd	%f42,%f58,%f58
		fmuld	%f22,%f14,%f62
	sllx	%o1,16,%o1
		faddd	%f24,%f48,%f48
	sllx	%o2,32,%o2
		faddd	%f26,%f50,%f50
	sllx	%o3,48,%o7
	or	%o1,%o0,%o0
		faddd	%f44,%f60,%f24	! %f60
	or	%o2,%o0,%o0
		faddd	%f46,%f62,%f26	! %f62
	or	%o7,%o0,%o0		! 64-bit result
		faddd	%f52,%f56,%f52
	addcc	%g1,%o0,%o0
	ldx	[%l0+8],%o7		! tp[j]
		faddd	%f54,%f58,%f54
	srlx	%o3,16,%g1		! 34-bit carry
		fdtox	%f48,%f48
	bcs,a	%xcc,.+8
	add	%g1,1,%g1
		fdtox	%f50,%f50
	addcc	%o7,%o0,%o0
		fdtox	%f52,%f52
	bcs,a	%xcc,.+8
	add	%g1,1,%g1

	stx	%o0,[%l0]		! tp[j-1]
		fdtox	%f54,%f54

	std	%f48,[%sp+0+128+0]
	std	%f50,[%sp+0+128+8]
	std	%f52,[%sp+0+128+16]
	addcc	%l6,8,%l6
	std	%f54,[%sp+0+128+24]
	bnz,pt	%icc,.Linner
	add	%l0,8,%l0

.Linnerskip:
	fdtox	%f24,%f24
	fdtox	%f26,%f26

	ldx	[%sp+0+128+0],%o0
	ldx	[%sp+0+128+8],%o1
	ldx	[%sp+0+128+16],%o2
	ldx	[%sp+0+128+24],%o3

	srlx	%o0,16,%o7
	std	%f24,[%sp+0+128+32]
	add	%o7,%o1,%o1
	std	%f26,[%sp+0+128+40]
	srlx	%o1,16,%o7
	add	%o7,%o2,%o2
	srlx	%o2,16,%o7
	add	%o7,%o3,%o3		! %o3.%o2[0..15].%o1[0..15].%o0[0..15]
	and	%o0,%l7,%o0
	and	%o1,%l7,%o1
	and	%o2,%l7,%o2
	sllx	%o1,16,%o1
	sllx	%o2,32,%o2
	sllx	%o3,48,%o7
	or	%o1,%o0,%o0
	or	%o2,%o0,%o0
	ldx	[%sp+0+128+32],%o4
	or	%o7,%o0,%o0		! 64-bit result
	ldx	[%sp+0+128+40],%o5
	addcc	%g1,%o0,%o0
	ldx	[%l0+8],%o7		! tp[j]
	srlx	%o3,16,%g1		! 34-bit carry
	bcs,a	%xcc,.+8
	add	%g1,1,%g1

	addcc	%o7,%o0,%o0
	bcs,a	%xcc,.+8
	add	%g1,1,%g1

	stx	%o0,[%l0]		! tp[j-1]
	add	%l0,8,%l0

	srlx	%o4,16,%o7
	add	%o7,%o5,%o5
	and	%o4,%l7,%o4
	sllx	%o5,16,%o7
	or	%o7,%o4,%o4
	addcc	%g1,%o4,%o4
	srlx	%o5,48,%g1
	bcs,a	%xcc,.+8
	add	%g1,1,%g1

	addcc	%i4,%o4,%o4
	stx	%o4,[%l0]		! tp[num-1]
	mov	%g1,%i4
	bcs,a	%xcc,.+8
	add	%i4,1,%i4

	addcc	%l5,8,%l5
	bnz	%icc,.Louter
	nop

	add	%l0,8,%l0		! adjust tp to point at the end
	orn	%g0,%g0,%g4
	sub	%g0,%i5,%o7		! n=-num
	ba	.Lsub
	subcc	%g0,%g0,%g0		! clear %icc.c

.align	32
.Lsub:
	ldx	[%l0+%o7],%o0
	add	%i3,%o7,%g1
	ld	[%g1+0],%o2
	ld	[%g1+4],%o3
	srlx	%o0,32,%o1
	subccc	%o0,%o2,%o2
	add	%i0,%o7,%g1
	subccc	%o1,%o3,%o3
	st	%o2,[%g1+0]
	add	%o7,8,%o7
	brnz,pt	%o7,.Lsub
	st	%o3,[%g1+4]
	subc	%i4,0,%g4
	sub	%g0,%i5,%o7		! n=-num
	ba	.Lcopy
	nop

.align	32
.Lcopy:
	ldx	[%l0+%o7],%o0
	add	%i0,%o7,%g1
	ld	[%g1+0],%o2
	ld	[%g1+4],%o3
	stx	%g0,[%l0+%o7]
	and	%o0,%g4,%o0
	srlx	%o0,32,%o1
	andn	%o2,%g4,%o2
	andn	%o3,%g4,%o3
	or	%o2,%o0,%o0
	or	%o3,%o1,%o1
	st	%o0,[%g1+0]
	add	%o7,8,%o7
	brnz,pt	%o7,.Lcopy
	st	%o1,[%g1+4]
	sub	%g0,%i5,%o7		! n=-num

.Lzap:
	stx	%g0,[%l1+%o7]
	stx	%g0,[%l2+%o7]
	stx	%g0,[%l3+%o7]
	stx	%g0,[%l4+%o7]
	add	%o7,8,%o7
	brnz,pt	%o7,.Lzap
	nop

	ldx	[%sp+0+128+48],%o7
	wr	%g0,%o7,%asi		! restore %asi

	mov	1,%i0
.Lret:
	ret
	restore
.type   bn_mul_mont_fpu,#function
.size	bn_mul_mont_fpu,(.-bn_mul_mont_fpu)
.asciz	"Montgomery Multipltication for UltraSPARC, CRYPTOGAMS by <appro@openssl.org>"
.align	32
