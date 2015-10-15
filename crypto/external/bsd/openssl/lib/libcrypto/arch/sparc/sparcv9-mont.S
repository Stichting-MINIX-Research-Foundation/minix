.section	".text",#alloc,#execinstr

.global	bn_mul_mont_int
.align	32
bn_mul_mont_int:
	cmp	%o5,4			! 128 bits minimum
	bge,pt	%icc,.Lenter
	sethi	%hi(0xffffffff),%g1
	retl
	clr	%o0
.align	32
.Lenter:
	save	%sp,-128,%sp
	sll	%i5,2,%i5		! num*=4
	or	%g1,%lo(0xffffffff),%g1
	ld	[%i4],%i4
	cmp	%i1,%i2
	and	%i5,%g1,%i5
	ld	[%i2],%l2		! bp[0]
	nop

	add	%sp,0,%o7		! real top of stack
	ld	[%i1],%o0		! ap[0] ! redundant in squaring context
	sub	%o7,%i5,%o7
	ld	[%i1+4],%l5		! ap[1]
	and	%o7,-1024,%o7
	ld	[%i3],%o1		! np[0]
	sub	%o7,0,%sp		! alloca
	ld	[%i3+4],%l6		! np[1]
	be,pt	%icc,.Lbn_sqr_mont
	mov	12,%l1

	mulx	%o0,%l2,%o0	! ap[0]*bp[0]
	mulx	%l5,%l2,%g4	!prologue! ap[1]*bp[0]
	and	%o0,%g1,%o3
	add	%sp,0+128,%l4
	ld	[%i1+8],%l5		!prologue!

	mulx	%i4,%o3,%l3		! "t[0]"*n0
	and	%l3,%g1,%l3

	mulx	%o1,%l3,%o1	! np[0]*"t[0]"*n0
	mulx	%l6,%l3,%o4	!prologue! np[1]*"t[0]"*n0
	srlx	%o0,32,%o0
	add	%o3,%o1,%o1
	ld	[%i3+8],%l6		!prologue!
	srlx	%o1,32,%o1
	mov	%g4,%o3		!prologue!

.L1st:
	mulx	%l5,%l2,%g4
	mulx	%l6,%l3,%g5
	add	%o3,%o0,%o0
	ld	[%i1+%l1],%l5		! ap[j]
	and	%o0,%g1,%o3
	add	%o4,%o1,%o1
	ld	[%i3+%l1],%l6		! np[j]
	srlx	%o0,32,%o0
	add	%o3,%o1,%o1
	add	%l1,4,%l1			! j++
	mov	%g4,%o3
	st	%o1,[%l4]
	cmp	%l1,%i5
	mov	%g5,%o4
	srlx	%o1,32,%o1
	bl	%icc,.L1st
	add	%l4,4,%l4		! tp++
!.L1st

	mulx	%l5,%l2,%g4	!epilogue!
	mulx	%l6,%l3,%g5
	add	%o3,%o0,%o0
	and	%o0,%g1,%o3
	add	%o4,%o1,%o1
	srlx	%o0,32,%o0
	add	%o3,%o1,%o1
	st	%o1,[%l4]
	srlx	%o1,32,%o1

	add	%g4,%o0,%o0
	and	%o0,%g1,%o3
	add	%g5,%o1,%o1
	srlx	%o0,32,%o0
	add	%o3,%o1,%o1
	st	%o1,[%l4+4]
	srlx	%o1,32,%o1

	add	%o0,%o1,%o1
	st	%o1,[%l4+8]
	srlx	%o1,32,%o2

	mov	4,%l0			! i++
	ld	[%i2+4],%l2		! bp[1]
.Louter:
	add	%sp,0+128,%l4
	ld	[%i1],%o0		! ap[0]
	ld	[%i1+4],%l5		! ap[1]
	ld	[%i3],%o1		! np[0]
	ld	[%i3+4],%l6		! np[1]
	ld	[%l4],%g5		! tp[0]
	ld	[%l4+4],%l7		! tp[1]
	mov	12,%l1

	mulx	%o0,%l2,%o0
	mulx	%l5,%l2,%g4	!prologue!
	add	%g5,%o0,%o0
	ld	[%i1+8],%l5		!prologue!
	and	%o0,%g1,%o3

	mulx	%i4,%o3,%l3
	and	%l3,%g1,%l3

	mulx	%o1,%l3,%o1
	mulx	%l6,%l3,%o4	!prologue!
	srlx	%o0,32,%o0
	add	%o3,%o1,%o1
	ld	[%i3+8],%l6		!prologue!
	srlx	%o1,32,%o1
	mov	%g4,%o3		!prologue!

.Linner:
	mulx	%l5,%l2,%g4
	mulx	%l6,%l3,%g5
	add	%l7,%o0,%o0
	ld	[%i1+%l1],%l5		! ap[j]
	add	%o3,%o0,%o0
	add	%o4,%o1,%o1
	ld	[%i3+%l1],%l6		! np[j]
	and	%o0,%g1,%o3
	ld	[%l4+8],%l7		! tp[j]
	srlx	%o0,32,%o0
	add	%o3,%o1,%o1
	add	%l1,4,%l1			! j++
	mov	%g4,%o3
	st	%o1,[%l4]		! tp[j-1]
	srlx	%o1,32,%o1
	mov	%g5,%o4
	cmp	%l1,%i5
	bl	%icc,.Linner
	add	%l4,4,%l4		! tp++
!.Linner

	mulx	%l5,%l2,%g4	!epilogue!
	mulx	%l6,%l3,%g5
	add	%l7,%o0,%o0
	add	%o3,%o0,%o0
	ld	[%l4+8],%l7		! tp[j]
	and	%o0,%g1,%o3
	add	%o4,%o1,%o1
	srlx	%o0,32,%o0
	add	%o3,%o1,%o1
	st	%o1,[%l4]		! tp[j-1]
	srlx	%o1,32,%o1

	add	%l7,%o0,%o0
	add	%g4,%o0,%o0
	and	%o0,%g1,%o3
	add	%g5,%o1,%o1
	add	%o3,%o1,%o1
	st	%o1,[%l4+4]		! tp[j-1]
	srlx	%o0,32,%o0
	add	%l0,4,%l0			! i++
	srlx	%o1,32,%o1

	add	%o0,%o1,%o1
	cmp	%l0,%i5
	add	%o2,%o1,%o1
	st	%o1,[%l4+8]

	srlx	%o1,32,%o2
	bl,a	%icc,.Louter
	ld	[%i2+%l0],%l2		! bp[i]
!.Louter

	add	%l4,12,%l4

.Ltail:
	add	%i3,%i5,%i3
	add	%i0,%i5,%i0
	mov	%l4,%i1
	sub	%g0,%i5,%o7		! k=-num
	ba	.Lsub
	subcc	%g0,%g0,%g0		! clear %icc.c
.align	16
.Lsub:
	ld	[%l4+%o7],%o0
	ld	[%i3+%o7],%o1
	subccc	%o0,%o1,%o1		! tp[j]-np[j]
	add	%i0,%o7,%l0
	add	%o7,4,%o7
	brnz	%o7,.Lsub
	st	%o1,[%l0]
	subc	%o2,0,%o2		! handle upmost overflow bit
	and	%l4,%o2,%i1
	andn	%i0,%o2,%i3
	or	%i1,%i3,%i1
	sub	%g0,%i5,%o7

.Lcopy:
	ld	[%i1+%o7],%o0		! copy or in-place refresh
	st	%g0,[%l4+%o7]		! zap tp
	st	%o0,[%i0+%o7]
	add	%o7,4,%o7
	brnz	%o7,.Lcopy
	nop
	mov	1,%i0
	ret
	restore
.align	32
.Lbn_sqr_mont:
	mulx	%l2,%l2,%o0		! ap[0]*ap[0]
	mulx	%l5,%l2,%g4		!prologue!
	and	%o0,%g1,%o3
	add	%sp,0+128,%l4
	ld	[%i1+8],%l5			!prologue!

	mulx	%i4,%o3,%l3			! "t[0]"*n0
	srlx	%o0,32,%o0
	and	%l3,%g1,%l3

	mulx	%o1,%l3,%o1		! np[0]*"t[0]"*n0
	mulx	%l6,%l3,%o4		!prologue!
	and	%o0,1,%i2
	ld	[%i3+8],%l6			!prologue!
	srlx	%o0,1,%o0
	add	%o3,%o1,%o1
	srlx	%o1,32,%o1
	mov	%g4,%o3			!prologue!

.Lsqr_1st:
	mulx	%l5,%l2,%g4
	mulx	%l6,%l3,%g5
	add	%o3,%o0,%o0		! ap[j]*a0+c0
	add	%o4,%o1,%o1
	ld	[%i1+%l1],%l5			! ap[j]
	and	%o0,%g1,%o3
	ld	[%i3+%l1],%l6			! np[j]
	srlx	%o0,32,%o0
	add	%o3,%o3,%o3
	or	%i2,%o3,%o3
	mov	%g5,%o4
	srlx	%o3,32,%i2
	add	%l1,4,%l1				! j++
	and	%o3,%g1,%o3
	cmp	%l1,%i5
	add	%o3,%o1,%o1
	st	%o1,[%l4]
	mov	%g4,%o3
	srlx	%o1,32,%o1
	bl	%icc,.Lsqr_1st
	add	%l4,4,%l4			! tp++
!.Lsqr_1st

	mulx	%l5,%l2,%g4		! epilogue
	mulx	%l6,%l3,%g5
	add	%o3,%o0,%o0		! ap[j]*a0+c0
	add	%o4,%o1,%o1
	and	%o0,%g1,%o3
	srlx	%o0,32,%o0
	add	%o3,%o3,%o3
	or	%i2,%o3,%o3
	srlx	%o3,32,%i2
	and	%o3,%g1,%o3
	add	%o3,%o1,%o1
	st	%o1,[%l4]
	srlx	%o1,32,%o1

	add	%g4,%o0,%o0		! ap[j]*a0+c0
	add	%g5,%o1,%o1
	and	%o0,%g1,%o3
	srlx	%o0,32,%o0
	add	%o3,%o3,%o3
	or	%i2,%o3,%o3
	srlx	%o3,32,%i2
	and	%o3,%g1,%o3
	add	%o3,%o1,%o1
	st	%o1,[%l4+4]
	srlx	%o1,32,%o1

	add	%o0,%o0,%o0
	or	%i2,%o0,%o0
	add	%o0,%o1,%o1
	st	%o1,[%l4+8]
	srlx	%o1,32,%o2

	ld	[%sp+0+128],%g4	! tp[0]
	ld	[%sp+0+128+4],%g5	! tp[1]
	ld	[%sp+0+128+8],%l7	! tp[2]
	ld	[%i1+4],%l2			! ap[1]
	ld	[%i1+8],%l5			! ap[2]
	ld	[%i3],%o1			! np[0]
	ld	[%i3+4],%l6			! np[1]
	mulx	%i4,%g4,%l3

	mulx	%l2,%l2,%o0
	and	%l3,%g1,%l3

	mulx	%o1,%l3,%o1
	mulx	%l6,%l3,%o4
	add	%g4,%o1,%o1
	and	%o0,%g1,%o3
	ld	[%i3+8],%l6			! np[2]
	srlx	%o1,32,%o1
	add	%g5,%o1,%o1
	srlx	%o0,32,%o0
	add	%o3,%o1,%o1
	and	%o0,1,%i2
	add	%o4,%o1,%o1
	srlx	%o0,1,%o0
	mov	12,%l1
	st	%o1,[%sp+0+128]	! tp[0]=
	srlx	%o1,32,%o1
	add	%sp,0+128+4,%l4

.Lsqr_2nd:
	mulx	%l5,%l2,%o3
	mulx	%l6,%l3,%o4
	add	%o3,%o0,%o0
	add	%l7,%o1,%o1
	ld	[%i1+%l1],%l5			! ap[j]
	and	%o0,%g1,%o3
	ld	[%i3+%l1],%l6			! np[j]
	srlx	%o0,32,%o0
	add	%o4,%o1,%o1
	ld	[%l4+8],%l7			! tp[j]
	add	%o3,%o3,%o3
	add	%l1,4,%l1				! j++
	or	%i2,%o3,%o3
	srlx	%o3,32,%i2
	and	%o3,%g1,%o3
	cmp	%l1,%i5
	add	%o3,%o1,%o1
	st	%o1,[%l4]			! tp[j-1]
	srlx	%o1,32,%o1
	bl	%icc,.Lsqr_2nd
	add	%l4,4,%l4			! tp++
!.Lsqr_2nd

	mulx	%l5,%l2,%o3
	mulx	%l6,%l3,%o4
	add	%o3,%o0,%o0
	add	%l7,%o1,%o1
	and	%o0,%g1,%o3
	srlx	%o0,32,%o0
	add	%o4,%o1,%o1
	add	%o3,%o3,%o3
	or	%i2,%o3,%o3
	srlx	%o3,32,%i2
	and	%o3,%g1,%o3
	add	%o3,%o1,%o1
	st	%o1,[%l4]			! tp[j-1]
	srlx	%o1,32,%o1

	add	%o0,%o0,%o0
	or	%i2,%o0,%o0
	add	%o0,%o1,%o1
	add	%o2,%o1,%o1
	st	%o1,[%l4+4]
	srlx	%o1,32,%o2

	ld	[%sp+0+128],%g5	! tp[0]
	ld	[%sp+0+128+4],%l7	! tp[1]
	ld	[%i1+8],%l2			! ap[2]
	ld	[%i3],%o1			! np[0]
	ld	[%i3+4],%l6			! np[1]
	mulx	%i4,%g5,%l3
	and	%l3,%g1,%l3
	mov	8,%l0

	mulx	%l2,%l2,%o0
	mulx	%o1,%l3,%o1
	and	%o0,%g1,%o3
	add	%g5,%o1,%o1
	srlx	%o0,32,%o0
	add	%sp,0+128,%l4
	srlx	%o1,32,%o1
	and	%o0,1,%i2
	srlx	%o0,1,%o0
	mov	4,%l1

.Lsqr_outer:
.Lsqr_inner1:
	mulx	%l6,%l3,%o4
	add	%l7,%o1,%o1
	add	%l1,4,%l1
	ld	[%l4+8],%l7
	cmp	%l1,%l0
	add	%o4,%o1,%o1
	ld	[%i3+%l1],%l6
	st	%o1,[%l4]
	srlx	%o1,32,%o1
	bl	%icc,.Lsqr_inner1
	add	%l4,4,%l4
!.Lsqr_inner1

	add	%l1,4,%l1
	ld	[%i1+%l1],%l5			! ap[j]
	mulx	%l6,%l3,%o4
	add	%l7,%o1,%o1
	ld	[%i3+%l1],%l6			! np[j]
	add	%o3,%o1,%o1
	ld	[%l4+8],%l7			! tp[j]
	add	%o4,%o1,%o1
	st	%o1,[%l4]
	srlx	%o1,32,%o1

	add	%l1,4,%l1
	cmp	%l1,%i5
	be,pn	%icc,.Lsqr_no_inner2
	add	%l4,4,%l4

.Lsqr_inner2:
	mulx	%l5,%l2,%o3
	mulx	%l6,%l3,%o4
	add	%l7,%o1,%o1
	add	%o3,%o0,%o0
	ld	[%i1+%l1],%l5			! ap[j]
	and	%o0,%g1,%o3
	ld	[%i3+%l1],%l6			! np[j]
	srlx	%o0,32,%o0
	add	%o3,%o3,%o3
	ld	[%l4+8],%l7			! tp[j]
	or	%i2,%o3,%o3
	add	%l1,4,%l1				! j++
	srlx	%o3,32,%i2
	and	%o3,%g1,%o3
	cmp	%l1,%i5
	add	%o3,%o1,%o1
	add	%o4,%o1,%o1
	st	%o1,[%l4]			! tp[j-1]
	srlx	%o1,32,%o1
	bl	%icc,.Lsqr_inner2
	add	%l4,4,%l4			! tp++

.Lsqr_no_inner2:
	mulx	%l5,%l2,%o3
	mulx	%l6,%l3,%o4
	add	%l7,%o1,%o1
	add	%o3,%o0,%o0
	and	%o0,%g1,%o3
	srlx	%o0,32,%o0
	add	%o3,%o3,%o3
	or	%i2,%o3,%o3
	srlx	%o3,32,%i2
	and	%o3,%g1,%o3
	add	%o3,%o1,%o1
	add	%o4,%o1,%o1
	st	%o1,[%l4]			! tp[j-1]
	srlx	%o1,32,%o1

	add	%o0,%o0,%o0
	or	%i2,%o0,%o0
	add	%o0,%o1,%o1
	add	%o2,%o1,%o1
	st	%o1,[%l4+4]
	srlx	%o1,32,%o2

	add	%l0,4,%l0				! i++
	ld	[%sp+0+128],%g5	! tp[0]
	ld	[%sp+0+128+4],%l7	! tp[1]
	ld	[%i1+%l0],%l2			! ap[j]
	ld	[%i3],%o1			! np[0]
	ld	[%i3+4],%l6			! np[1]
	mulx	%i4,%g5,%l3
	and	%l3,%g1,%l3
	add	%l0,4,%g4

	mulx	%l2,%l2,%o0
	mulx	%o1,%l3,%o1
	and	%o0,%g1,%o3
	add	%g5,%o1,%o1
	srlx	%o0,32,%o0
	add	%sp,0+128,%l4
	srlx	%o1,32,%o1
	and	%o0,1,%i2
	srlx	%o0,1,%o0

	cmp	%g4,%i5			! i<num-1
	bl	%icc,.Lsqr_outer
	mov	4,%l1

.Lsqr_last:
	mulx	%l6,%l3,%o4
	add	%l7,%o1,%o1
	add	%l1,4,%l1
	ld	[%l4+8],%l7
	cmp	%l1,%l0
	add	%o4,%o1,%o1
	ld	[%i3+%l1],%l6
	st	%o1,[%l4]
	srlx	%o1,32,%o1
	bl	%icc,.Lsqr_last
	add	%l4,4,%l4
!.Lsqr_last

	mulx	%l6,%l3,%o4
	add	%l7,%o1,%o1
	add	%o3,%o1,%o1
	add	%o4,%o1,%o1
	st	%o1,[%l4]
	srlx	%o1,32,%o1

	add	%o0,%o0,%o0		! recover %o0
	or	%i2,%o0,%o0
	add	%o0,%o1,%o1
	add	%o2,%o1,%o1
	st	%o1,[%l4+4]
	srlx	%o1,32,%o2

	ba	.Ltail
	add	%l4,8,%l4
.type	bn_mul_mont_int,#function
.size	bn_mul_mont_int,(.-bn_mul_mont_int)
.asciz	"Montgomery Multipltication for SPARCv9, CRYPTOGAMS by <appro@openssl.org>"
.align	32
