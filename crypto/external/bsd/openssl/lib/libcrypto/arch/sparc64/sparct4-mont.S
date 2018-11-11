#include "sparc_arch.h"

#ifdef	__arch64__
.register	%g2,#scratch
.register	%g3,#scratch
#endif

.section	".text",#alloc,#execinstr

#ifdef	__PIC__
SPARC_PIC_THUNK(%g1)
#endif
.globl	bn_mul_mont_t4_8
.align	32
bn_mul_mont_t4_8:
#ifdef	__arch64__
	mov	0,%g5
	mov	-128,%g4
#elif defined(SPARCV9_64BIT_STACK)
	SPARC_LOAD_ADDRESS_LEAF(OPENSSL_sparcv9cap_P,%g1,%g5)
	ld	[%g1+0],%g1	! OPENSSL_sparcv9_P[0]
	mov	-2047,%g4
	and	%g1,SPARCV9_64BIT_STACK,%g1
	movrz	%g1,0,%g4
	mov	-1,%g5
	add	%g4,-128,%g4
#else
	mov	-1,%g5
	mov	-128,%g4
#endif
	sllx	%g5,32,%g5
	save	%sp,%g4,%sp
#ifndef	__arch64__
	save	%sp,-128,%sp	! warm it up
	save	%sp,-128,%sp
	save	%sp,-128,%sp
	save	%sp,-128,%sp
	save	%sp,-128,%sp
	save	%sp,-128,%sp
	restore
	restore
	restore
	restore
	restore
	restore
#endif
	and	%sp,1,%g4
	or	%g5,%fp,%fp
	or	%g4,%g5,%g5

	! copy arguments to global registers
	mov	%i0,%g1
	mov	%i1,%g2
	mov	%i2,%g3
	mov	%i3,%g4
	ld	[%i4+0],%f1	! load *n0
	ld	[%i4+4],%f0
	.word	0xbbb00f00 !fsrc2	%f0,%f0,%f60
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	ld	[%g2+0*8+0],%l1
	ld	[%g2+0*8+4],%l0
	sllx	%l0,32,%l0
	or	%l1,%l0,%l0
	ld	[%g2+1*8+0],%l2
	ld	[%g2+1*8+4],%l1
	sllx	%l1,32,%l1
	or	%l2,%l1,%l1
	ld	[%g2+2*8+0],%l3
	ld	[%g2+2*8+4],%l2
	sllx	%l2,32,%l2
	or	%l3,%l2,%l2
	ld	[%g2+3*8+0],%l4
	ld	[%g2+3*8+4],%l3
	sllx	%l3,32,%l3
	or	%l4,%l3,%l3
	ld	[%g2+4*8+0],%l5
	ld	[%g2+4*8+4],%l4
	sllx	%l4,32,%l4
	or	%l5,%l4,%l4
	ld	[%g2+5*8+0],%l6
	ld	[%g2+5*8+4],%l5
	sllx	%l5,32,%l5
	or	%l6,%l5,%l5
	ld	[%g2+6*8+0],%l7
	ld	[%g2+6*8+4],%l6
	sllx	%l6,32,%l6
	or	%l7,%l6,%l6
	ld	[%g2+7*8+0],%o0
	ld	[%g2+7*8+4],%l7
	sllx	%l7,32,%l7
	or	%o0,%l7,%l7
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	ld	[%g4+0*8+0],%l1
	ld	[%g4+0*8+4],%l0
	sllx	%l0,32,%l0
	or	%l1,%l0,%l0
	ld	[%g4+1*8+0],%l2
	ld	[%g4+1*8+4],%l1
	sllx	%l1,32,%l1
	or	%l2,%l1,%l1
	ld	[%g4+2*8+0],%l3
	ld	[%g4+2*8+4],%l2
	sllx	%l2,32,%l2
	or	%l3,%l2,%l2
	ld	[%g4+3*8+0],%l4
	ld	[%g4+3*8+4],%l3
	sllx	%l3,32,%l3
	or	%l4,%l3,%l3
	ld	[%g4+4*8+0],%l5
	ld	[%g4+4*8+4],%l4
	sllx	%l4,32,%l4
	or	%l5,%l4,%l4
	ld	[%g4+5*8+0],%l6
	ld	[%g4+5*8+4],%l5
	sllx	%l5,32,%l5
	or	%l6,%l5,%l5
	ld	[%g4+6*8+0],%l7
	ld	[%g4+6*8+4],%l6
	sllx	%l6,32,%l6
	or	%l7,%l6,%l6
	ld	[%g4+7*8+0],%o0
	ld	[%g4+7*8+4],%l7
	sllx	%l7,32,%l7
	or	%o0,%l7,%l7
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	cmp	%g2,%g3
	be	SIZE_T_CC,.Lmsquare_8
	nop
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	ld	[%g3+0*8+0],%i1
	ld	[%g3+0*8+4],%i0
	sllx	%i0,32,%i0
	or	%i1,%i0,%i0
	ld	[%g3+1*8+0],%i2
	ld	[%g3+1*8+4],%i1
	sllx	%i1,32,%i1
	or	%i2,%i1,%i1
	ld	[%g3+2*8+0],%i3
	ld	[%g3+2*8+4],%i2
	sllx	%i2,32,%i2
	or	%i3,%i2,%i2
	ld	[%g3+3*8+0],%i4
	ld	[%g3+3*8+4],%i3
	sllx	%i3,32,%i3
	or	%i4,%i3,%i3
	ld	[%g3+4*8+0],%i5
	ld	[%g3+4*8+4],%i4
	sllx	%i4,32,%i4
	or	%i5,%i4,%i4
	ld	[%g3+5*8+0],%l0
	ld	[%g3+5*8+4],%i5
	sllx	%i5,32,%i5
	or	%l0,%i5,%i5
	ld	[%g3+6*8+0],%l1
	ld	[%g3+6*8+4],%l0
	sllx	%l0,32,%l0
	or	%l1,%l0,%l0
	ld	[%g3+7*8+0],%l2
	ld	[%g3+7*8+4],%l1
	sllx	%l1,32,%l1
	or	%l2,%l1,%l1
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	.word	0x81b02920+8-1	! montmul	8-1
.Lmresume_8:
	fbu,pn	%fcc3,.Lmabort_8
#ifndef	__arch64__
	and	%fp,%g5,%g5
	brz,pn	%g5,.Lmabort_8
#endif
	nop
#ifdef	__arch64__
	restore
	restore
	restore
	restore
	restore
#else
	restore;		and	%fp,%g5,%g5
	restore;		and	%fp,%g5,%g5
	restore;		and	%fp,%g5,%g5
	restore;		and	%fp,%g5,%g5
	 brz,pn	%g5,.Lmabort1_8
	restore
#endif
	.word	0x81b02310 !movxtod	%l0,%f0
	.word	0x85b02311 !movxtod	%l1,%f2
	.word	0x89b02312 !movxtod	%l2,%f4
	.word	0x8db02313 !movxtod	%l3,%f6
	.word	0x91b02314 !movxtod	%l4,%f8
	.word	0x95b02315 !movxtod	%l5,%f10
	.word	0x99b02316 !movxtod	%l6,%f12
	.word	0x9db02317 !movxtod	%l7,%f14
#ifdef	__arch64__
	restore
#else
	 and	%fp,%g5,%g5
	restore
	 and	%g5,1,%o7
	 and	%fp,%g5,%g5
	 srl	%fp,0,%fp		! just in case?
	 or	%o7,%g5,%g5
	brz,a,pn %g5,.Lmdone_8
	mov	0,%i0		! return failure
#endif
	st	%f1,[%g1+0*8+0]
	st	%f0,[%g1+0*8+4]
	st	%f3,[%g1+1*8+0]
	st	%f2,[%g1+1*8+4]
	st	%f5,[%g1+2*8+0]
	st	%f4,[%g1+2*8+4]
	st	%f7,[%g1+3*8+0]
	st	%f6,[%g1+3*8+4]
	st	%f9,[%g1+4*8+0]
	st	%f8,[%g1+4*8+4]
	st	%f11,[%g1+5*8+0]
	st	%f10,[%g1+5*8+4]
	st	%f13,[%g1+6*8+0]
	st	%f12,[%g1+6*8+4]
	st	%f15,[%g1+7*8+0]
	st	%f14,[%g1+7*8+4]
	mov	1,%i0		! return success
.Lmdone_8:
	ret
	restore

.Lmabort_8:
	restore
	restore
	restore
	restore
	restore
.Lmabort1_8:
	restore

	mov	0,%i0		! return failure
	ret
	restore

.align	32
.Lmsquare_8:
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	.word   0x81b02940+8-1	! montsqr	8-1
	ba	.Lmresume_8
	nop
.type	bn_mul_mont_t4_8, #function
.size	bn_mul_mont_t4_8, .-bn_mul_mont_t4_8
.globl	bn_mul_mont_t4_16
.align	32
bn_mul_mont_t4_16:
#ifdef	__arch64__
	mov	0,%g5
	mov	-128,%g4
#elif defined(SPARCV9_64BIT_STACK)
	SPARC_LOAD_ADDRESS_LEAF(OPENSSL_sparcv9cap_P,%g1,%g5)
	ld	[%g1+0],%g1	! OPENSSL_sparcv9_P[0]
	mov	-2047,%g4
	and	%g1,SPARCV9_64BIT_STACK,%g1
	movrz	%g1,0,%g4
	mov	-1,%g5
	add	%g4,-128,%g4
#else
	mov	-1,%g5
	mov	-128,%g4
#endif
	sllx	%g5,32,%g5
	save	%sp,%g4,%sp
#ifndef	__arch64__
	save	%sp,-128,%sp	! warm it up
	save	%sp,-128,%sp
	save	%sp,-128,%sp
	save	%sp,-128,%sp
	save	%sp,-128,%sp
	save	%sp,-128,%sp
	restore
	restore
	restore
	restore
	restore
	restore
#endif
	and	%sp,1,%g4
	or	%g5,%fp,%fp
	or	%g4,%g5,%g5

	! copy arguments to global registers
	mov	%i0,%g1
	mov	%i1,%g2
	mov	%i2,%g3
	mov	%i3,%g4
	ld	[%i4+0],%f1	! load *n0
	ld	[%i4+4],%f0
	.word	0xbbb00f00 !fsrc2	%f0,%f0,%f60
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	ld	[%g2+0*8+0],%l1
	ld	[%g2+0*8+4],%l0
	sllx	%l0,32,%l0
	or	%l1,%l0,%l0
	ld	[%g2+1*8+0],%l2
	ld	[%g2+1*8+4],%l1
	sllx	%l1,32,%l1
	or	%l2,%l1,%l1
	ld	[%g2+2*8+0],%l3
	ld	[%g2+2*8+4],%l2
	sllx	%l2,32,%l2
	or	%l3,%l2,%l2
	ld	[%g2+3*8+0],%l4
	ld	[%g2+3*8+4],%l3
	sllx	%l3,32,%l3
	or	%l4,%l3,%l3
	ld	[%g2+4*8+0],%l5
	ld	[%g2+4*8+4],%l4
	sllx	%l4,32,%l4
	or	%l5,%l4,%l4
	ld	[%g2+5*8+0],%l6
	ld	[%g2+5*8+4],%l5
	sllx	%l5,32,%l5
	or	%l6,%l5,%l5
	ld	[%g2+6*8+0],%l7
	ld	[%g2+6*8+4],%l6
	sllx	%l6,32,%l6
	or	%l7,%l6,%l6
	ld	[%g2+7*8+0],%o0
	ld	[%g2+7*8+4],%l7
	sllx	%l7,32,%l7
	or	%o0,%l7,%l7
	ld	[%g2+8*8+0],%o1
	ld	[%g2+8*8+4],%o0
	sllx	%o0,32,%o0
	or	%o1,%o0,%o0
	ld	[%g2+9*8+0],%o2
	ld	[%g2+9*8+4],%o1
	sllx	%o1,32,%o1
	or	%o2,%o1,%o1
	ld	[%g2+10*8+0],%o3
	ld	[%g2+10*8+4],%o2
	sllx	%o2,32,%o2
	or	%o3,%o2,%o2
	ld	[%g2+11*8+0],%o4
	ld	[%g2+11*8+4],%o3
	sllx	%o3,32,%o3
	or	%o4,%o3,%o3
	ld	[%g2+12*8+0],%o5
	ld	[%g2+12*8+4],%o4
	sllx	%o4,32,%o4
	or	%o5,%o4,%o4
	ld	[%g2+13*8+0],%o7
	ld	[%g2+13*8+4],%o5
	sllx	%o5,32,%o5
	or	%o7,%o5,%o5
	ld	[%g2+14*8+0],%f5
	ld	[%g2+14*8+4],%f4
	.word	0xb1b00f04 !fsrc2	%f0,%f4,%f24
	ld	[%g2+15*8+0],%f7
	ld	[%g2+15*8+4],%f6
	.word	0xb5b00f06 !fsrc2	%f0,%f6,%f26
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	ld	[%g4+0*8+0],%l1
	ld	[%g4+0*8+4],%l0
	sllx	%l0,32,%l0
	or	%l1,%l0,%l0
	ld	[%g4+1*8+0],%l2
	ld	[%g4+1*8+4],%l1
	sllx	%l1,32,%l1
	or	%l2,%l1,%l1
	ld	[%g4+2*8+0],%l3
	ld	[%g4+2*8+4],%l2
	sllx	%l2,32,%l2
	or	%l3,%l2,%l2
	ld	[%g4+3*8+0],%l4
	ld	[%g4+3*8+4],%l3
	sllx	%l3,32,%l3
	or	%l4,%l3,%l3
	ld	[%g4+4*8+0],%l5
	ld	[%g4+4*8+4],%l4
	sllx	%l4,32,%l4
	or	%l5,%l4,%l4
	ld	[%g4+5*8+0],%l6
	ld	[%g4+5*8+4],%l5
	sllx	%l5,32,%l5
	or	%l6,%l5,%l5
	ld	[%g4+6*8+0],%l7
	ld	[%g4+6*8+4],%l6
	sllx	%l6,32,%l6
	or	%l7,%l6,%l6
	ld	[%g4+7*8+0],%o0
	ld	[%g4+7*8+4],%l7
	sllx	%l7,32,%l7
	or	%o0,%l7,%l7
	ld	[%g4+8*8+0],%o1
	ld	[%g4+8*8+4],%o0
	sllx	%o0,32,%o0
	or	%o1,%o0,%o0
	ld	[%g4+9*8+0],%o2
	ld	[%g4+9*8+4],%o1
	sllx	%o1,32,%o1
	or	%o2,%o1,%o1
	ld	[%g4+10*8+0],%o3
	ld	[%g4+10*8+4],%o2
	sllx	%o2,32,%o2
	or	%o3,%o2,%o2
	ld	[%g4+11*8+0],%o4
	ld	[%g4+11*8+4],%o3
	sllx	%o3,32,%o3
	or	%o4,%o3,%o3
	ld	[%g4+12*8+0],%o5
	ld	[%g4+12*8+4],%o4
	sllx	%o4,32,%o4
	or	%o5,%o4,%o4
	ld	[%g4+13*8+0],%o7
	ld	[%g4+13*8+4],%o5
	sllx	%o5,32,%o5
	or	%o7,%o5,%o5
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	ld	[%g4+14*8+0],%l1
	ld	[%g4+14*8+4],%l0
	sllx	%l0,32,%l0
	or	%l1,%l0,%l0
	ld	[%g4+15*8+0],%l2
	ld	[%g4+15*8+4],%l1
	sllx	%l1,32,%l1
	or	%l2,%l1,%l1
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	cmp	%g2,%g3
	be	SIZE_T_CC,.Lmsquare_16
	nop
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	ld	[%g3+0*8+0],%i1
	ld	[%g3+0*8+4],%i0
	sllx	%i0,32,%i0
	or	%i1,%i0,%i0
	ld	[%g3+1*8+0],%i2
	ld	[%g3+1*8+4],%i1
	sllx	%i1,32,%i1
	or	%i2,%i1,%i1
	ld	[%g3+2*8+0],%i3
	ld	[%g3+2*8+4],%i2
	sllx	%i2,32,%i2
	or	%i3,%i2,%i2
	ld	[%g3+3*8+0],%i4
	ld	[%g3+3*8+4],%i3
	sllx	%i3,32,%i3
	or	%i4,%i3,%i3
	ld	[%g3+4*8+0],%i5
	ld	[%g3+4*8+4],%i4
	sllx	%i4,32,%i4
	or	%i5,%i4,%i4
	ld	[%g3+5*8+0],%l0
	ld	[%g3+5*8+4],%i5
	sllx	%i5,32,%i5
	or	%l0,%i5,%i5
	ld	[%g3+6*8+0],%l1
	ld	[%g3+6*8+4],%l0
	sllx	%l0,32,%l0
	or	%l1,%l0,%l0
	ld	[%g3+7*8+0],%l2
	ld	[%g3+7*8+4],%l1
	sllx	%l1,32,%l1
	or	%l2,%l1,%l1
	ld	[%g3+8*8+0],%l3
	ld	[%g3+8*8+4],%l2
	sllx	%l2,32,%l2
	or	%l3,%l2,%l2
	ld	[%g3+9*8+0],%l4
	ld	[%g3+9*8+4],%l3
	sllx	%l3,32,%l3
	or	%l4,%l3,%l3
	ld	[%g3+10*8+0],%l5
	ld	[%g3+10*8+4],%l4
	sllx	%l4,32,%l4
	or	%l5,%l4,%l4
	ld	[%g3+11*8+0],%l6
	ld	[%g3+11*8+4],%l5
	sllx	%l5,32,%l5
	or	%l6,%l5,%l5
	ld	[%g3+12*8+0],%l7
	ld	[%g3+12*8+4],%l6
	sllx	%l6,32,%l6
	or	%l7,%l6,%l6
	ld	[%g3+13*8+0],%o7
	ld	[%g3+13*8+4],%l7
	sllx	%l7,32,%l7
	or	%o7,%l7,%l7
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	ld	[%g3+14*8+0],%i1
	ld	[%g3+14*8+4],%i0
	sllx	%i0,32,%i0
	or	%i1,%i0,%i0
	ld	[%g3+15*8+0],%o7
	ld	[%g3+15*8+4],%i1
	sllx	%i1,32,%i1
	or	%o7,%i1,%i1
	.word	0x81b02920+16-1	! montmul	16-1
.Lmresume_16:
	fbu,pn	%fcc3,.Lmabort_16
#ifndef	__arch64__
	and	%fp,%g5,%g5
	brz,pn	%g5,.Lmabort_16
#endif
	nop
#ifdef	__arch64__
	restore
	restore
	restore
	restore
	restore
#else
	restore;		and	%fp,%g5,%g5
	restore;		and	%fp,%g5,%g5
	restore;		and	%fp,%g5,%g5
	restore;		and	%fp,%g5,%g5
	 brz,pn	%g5,.Lmabort1_16
	restore
#endif
	.word	0x81b02310 !movxtod	%l0,%f0
	.word	0x85b02311 !movxtod	%l1,%f2
	.word	0x89b02312 !movxtod	%l2,%f4
	.word	0x8db02313 !movxtod	%l3,%f6
	.word	0x91b02314 !movxtod	%l4,%f8
	.word	0x95b02315 !movxtod	%l5,%f10
	.word	0x99b02316 !movxtod	%l6,%f12
	.word	0x9db02317 !movxtod	%l7,%f14
	.word	0xa1b02308 !movxtod	%o0,%f16
	.word	0xa5b02309 !movxtod	%o1,%f18
	.word	0xa9b0230a !movxtod	%o2,%f20
	.word	0xadb0230b !movxtod	%o3,%f22
	.word	0xbbb0230c !movxtod	%o4,%f60
	.word	0xbfb0230d !movxtod	%o5,%f62
#ifdef	__arch64__
	restore
#else
	 and	%fp,%g5,%g5
	restore
	 and	%g5,1,%o7
	 and	%fp,%g5,%g5
	 srl	%fp,0,%fp		! just in case?
	 or	%o7,%g5,%g5
	brz,a,pn %g5,.Lmdone_16
	mov	0,%i0		! return failure
#endif
	st	%f1,[%g1+0*8+0]
	st	%f0,[%g1+0*8+4]
	st	%f3,[%g1+1*8+0]
	st	%f2,[%g1+1*8+4]
	st	%f5,[%g1+2*8+0]
	st	%f4,[%g1+2*8+4]
	st	%f7,[%g1+3*8+0]
	st	%f6,[%g1+3*8+4]
	st	%f9,[%g1+4*8+0]
	st	%f8,[%g1+4*8+4]
	st	%f11,[%g1+5*8+0]
	st	%f10,[%g1+5*8+4]
	st	%f13,[%g1+6*8+0]
	st	%f12,[%g1+6*8+4]
	st	%f15,[%g1+7*8+0]
	st	%f14,[%g1+7*8+4]
	st	%f17,[%g1+8*8+0]
	st	%f16,[%g1+8*8+4]
	st	%f19,[%g1+9*8+0]
	st	%f18,[%g1+9*8+4]
	st	%f21,[%g1+10*8+0]
	st	%f20,[%g1+10*8+4]
	st	%f23,[%g1+11*8+0]
	st	%f22,[%g1+11*8+4]
	.word	0x81b00f1d !fsrc2	%f0,%f60,%f0
	st	%f1,[%g1+12*8+0]
	st	%f0,[%g1+12*8+4]
	.word	0x85b00f1f !fsrc2	%f0,%f62,%f2
	st	%f3,[%g1+13*8+0]
	st	%f2,[%g1+13*8+4]
	.word	0x89b00f18 !fsrc2	%f0,%f24,%f4
	st	%f5,[%g1+14*8+0]
	st	%f4,[%g1+14*8+4]
	.word	0x8db00f1a !fsrc2	%f0,%f26,%f6
	st	%f7,[%g1+15*8+0]
	st	%f6,[%g1+15*8+4]
	mov	1,%i0		! return success
.Lmdone_16:
	ret
	restore

.Lmabort_16:
	restore
	restore
	restore
	restore
	restore
.Lmabort1_16:
	restore

	mov	0,%i0		! return failure
	ret
	restore

.align	32
.Lmsquare_16:
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	.word   0x81b02940+16-1	! montsqr	16-1
	ba	.Lmresume_16
	nop
.type	bn_mul_mont_t4_16, #function
.size	bn_mul_mont_t4_16, .-bn_mul_mont_t4_16
.globl	bn_mul_mont_t4_24
.align	32
bn_mul_mont_t4_24:
#ifdef	__arch64__
	mov	0,%g5
	mov	-128,%g4
#elif defined(SPARCV9_64BIT_STACK)
	SPARC_LOAD_ADDRESS_LEAF(OPENSSL_sparcv9cap_P,%g1,%g5)
	ld	[%g1+0],%g1	! OPENSSL_sparcv9_P[0]
	mov	-2047,%g4
	and	%g1,SPARCV9_64BIT_STACK,%g1
	movrz	%g1,0,%g4
	mov	-1,%g5
	add	%g4,-128,%g4
#else
	mov	-1,%g5
	mov	-128,%g4
#endif
	sllx	%g5,32,%g5
	save	%sp,%g4,%sp
#ifndef	__arch64__
	save	%sp,-128,%sp	! warm it up
	save	%sp,-128,%sp
	save	%sp,-128,%sp
	save	%sp,-128,%sp
	save	%sp,-128,%sp
	save	%sp,-128,%sp
	restore
	restore
	restore
	restore
	restore
	restore
#endif
	and	%sp,1,%g4
	or	%g5,%fp,%fp
	or	%g4,%g5,%g5

	! copy arguments to global registers
	mov	%i0,%g1
	mov	%i1,%g2
	mov	%i2,%g3
	mov	%i3,%g4
	ld	[%i4+0],%f1	! load *n0
	ld	[%i4+4],%f0
	.word	0xbbb00f00 !fsrc2	%f0,%f0,%f60
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	ld	[%g2+0*8+0],%l1
	ld	[%g2+0*8+4],%l0
	sllx	%l0,32,%l0
	or	%l1,%l0,%l0
	ld	[%g2+1*8+0],%l2
	ld	[%g2+1*8+4],%l1
	sllx	%l1,32,%l1
	or	%l2,%l1,%l1
	ld	[%g2+2*8+0],%l3
	ld	[%g2+2*8+4],%l2
	sllx	%l2,32,%l2
	or	%l3,%l2,%l2
	ld	[%g2+3*8+0],%l4
	ld	[%g2+3*8+4],%l3
	sllx	%l3,32,%l3
	or	%l4,%l3,%l3
	ld	[%g2+4*8+0],%l5
	ld	[%g2+4*8+4],%l4
	sllx	%l4,32,%l4
	or	%l5,%l4,%l4
	ld	[%g2+5*8+0],%l6
	ld	[%g2+5*8+4],%l5
	sllx	%l5,32,%l5
	or	%l6,%l5,%l5
	ld	[%g2+6*8+0],%l7
	ld	[%g2+6*8+4],%l6
	sllx	%l6,32,%l6
	or	%l7,%l6,%l6
	ld	[%g2+7*8+0],%o0
	ld	[%g2+7*8+4],%l7
	sllx	%l7,32,%l7
	or	%o0,%l7,%l7
	ld	[%g2+8*8+0],%o1
	ld	[%g2+8*8+4],%o0
	sllx	%o0,32,%o0
	or	%o1,%o0,%o0
	ld	[%g2+9*8+0],%o2
	ld	[%g2+9*8+4],%o1
	sllx	%o1,32,%o1
	or	%o2,%o1,%o1
	ld	[%g2+10*8+0],%o3
	ld	[%g2+10*8+4],%o2
	sllx	%o2,32,%o2
	or	%o3,%o2,%o2
	ld	[%g2+11*8+0],%o4
	ld	[%g2+11*8+4],%o3
	sllx	%o3,32,%o3
	or	%o4,%o3,%o3
	ld	[%g2+12*8+0],%o5
	ld	[%g2+12*8+4],%o4
	sllx	%o4,32,%o4
	or	%o5,%o4,%o4
	ld	[%g2+13*8+0],%o7
	ld	[%g2+13*8+4],%o5
	sllx	%o5,32,%o5
	or	%o7,%o5,%o5
	ld	[%g2+14*8+0],%f5
	ld	[%g2+14*8+4],%f4
	.word	0xb1b00f04 !fsrc2	%f0,%f4,%f24
	ld	[%g2+15*8+0],%f7
	ld	[%g2+15*8+4],%f6
	.word	0xb5b00f06 !fsrc2	%f0,%f6,%f26
	ld	[%g2+16*8+0],%f1
	ld	[%g2+16*8+4],%f0
	.word	0xb9b00f00 !fsrc2	%f0,%f0,%f28
	ld	[%g2+17*8+0],%f3
	ld	[%g2+17*8+4],%f2
	.word	0xbdb00f02 !fsrc2	%f0,%f2,%f30
	ld	[%g2+18*8+0],%f5
	ld	[%g2+18*8+4],%f4
	.word	0x83b00f04 !fsrc2	%f0,%f4,%f32
	ld	[%g2+19*8+0],%f7
	ld	[%g2+19*8+4],%f6
	.word	0x87b00f06 !fsrc2	%f0,%f6,%f34
	ld	[%g2+20*8+0],%f1
	ld	[%g2+20*8+4],%f0
	.word	0x8bb00f00 !fsrc2	%f0,%f0,%f36
	ld	[%g2+21*8+0],%f3
	ld	[%g2+21*8+4],%f2
	.word	0x8fb00f02 !fsrc2	%f0,%f2,%f38
	ld	[%g2+22*8+0],%f5
	ld	[%g2+22*8+4],%f4
	.word	0x93b00f04 !fsrc2	%f0,%f4,%f40
	ld	[%g2+23*8+0],%f7
	ld	[%g2+23*8+4],%f6
	.word	0x97b00f06 !fsrc2	%f0,%f6,%f42
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	ld	[%g4+0*8+0],%l1
	ld	[%g4+0*8+4],%l0
	sllx	%l0,32,%l0
	or	%l1,%l0,%l0
	ld	[%g4+1*8+0],%l2
	ld	[%g4+1*8+4],%l1
	sllx	%l1,32,%l1
	or	%l2,%l1,%l1
	ld	[%g4+2*8+0],%l3
	ld	[%g4+2*8+4],%l2
	sllx	%l2,32,%l2
	or	%l3,%l2,%l2
	ld	[%g4+3*8+0],%l4
	ld	[%g4+3*8+4],%l3
	sllx	%l3,32,%l3
	or	%l4,%l3,%l3
	ld	[%g4+4*8+0],%l5
	ld	[%g4+4*8+4],%l4
	sllx	%l4,32,%l4
	or	%l5,%l4,%l4
	ld	[%g4+5*8+0],%l6
	ld	[%g4+5*8+4],%l5
	sllx	%l5,32,%l5
	or	%l6,%l5,%l5
	ld	[%g4+6*8+0],%l7
	ld	[%g4+6*8+4],%l6
	sllx	%l6,32,%l6
	or	%l7,%l6,%l6
	ld	[%g4+7*8+0],%o0
	ld	[%g4+7*8+4],%l7
	sllx	%l7,32,%l7
	or	%o0,%l7,%l7
	ld	[%g4+8*8+0],%o1
	ld	[%g4+8*8+4],%o0
	sllx	%o0,32,%o0
	or	%o1,%o0,%o0
	ld	[%g4+9*8+0],%o2
	ld	[%g4+9*8+4],%o1
	sllx	%o1,32,%o1
	or	%o2,%o1,%o1
	ld	[%g4+10*8+0],%o3
	ld	[%g4+10*8+4],%o2
	sllx	%o2,32,%o2
	or	%o3,%o2,%o2
	ld	[%g4+11*8+0],%o4
	ld	[%g4+11*8+4],%o3
	sllx	%o3,32,%o3
	or	%o4,%o3,%o3
	ld	[%g4+12*8+0],%o5
	ld	[%g4+12*8+4],%o4
	sllx	%o4,32,%o4
	or	%o5,%o4,%o4
	ld	[%g4+13*8+0],%o7
	ld	[%g4+13*8+4],%o5
	sllx	%o5,32,%o5
	or	%o7,%o5,%o5
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	ld	[%g4+14*8+0],%l1
	ld	[%g4+14*8+4],%l0
	sllx	%l0,32,%l0
	or	%l1,%l0,%l0
	ld	[%g4+15*8+0],%l2
	ld	[%g4+15*8+4],%l1
	sllx	%l1,32,%l1
	or	%l2,%l1,%l1
	ld	[%g4+16*8+0],%l3
	ld	[%g4+16*8+4],%l2
	sllx	%l2,32,%l2
	or	%l3,%l2,%l2
	ld	[%g4+17*8+0],%l4
	ld	[%g4+17*8+4],%l3
	sllx	%l3,32,%l3
	or	%l4,%l3,%l3
	ld	[%g4+18*8+0],%l5
	ld	[%g4+18*8+4],%l4
	sllx	%l4,32,%l4
	or	%l5,%l4,%l4
	ld	[%g4+19*8+0],%l6
	ld	[%g4+19*8+4],%l5
	sllx	%l5,32,%l5
	or	%l6,%l5,%l5
	ld	[%g4+20*8+0],%l7
	ld	[%g4+20*8+4],%l6
	sllx	%l6,32,%l6
	or	%l7,%l6,%l6
	ld	[%g4+21*8+0],%o0
	ld	[%g4+21*8+4],%l7
	sllx	%l7,32,%l7
	or	%o0,%l7,%l7
	ld	[%g4+22*8+0],%o1
	ld	[%g4+22*8+4],%o0
	sllx	%o0,32,%o0
	or	%o1,%o0,%o0
	ld	[%g4+23*8+0],%o2
	ld	[%g4+23*8+4],%o1
	sllx	%o1,32,%o1
	or	%o2,%o1,%o1
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	cmp	%g2,%g3
	be	SIZE_T_CC,.Lmsquare_24
	nop
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	ld	[%g3+0*8+0],%i1
	ld	[%g3+0*8+4],%i0
	sllx	%i0,32,%i0
	or	%i1,%i0,%i0
	ld	[%g3+1*8+0],%i2
	ld	[%g3+1*8+4],%i1
	sllx	%i1,32,%i1
	or	%i2,%i1,%i1
	ld	[%g3+2*8+0],%i3
	ld	[%g3+2*8+4],%i2
	sllx	%i2,32,%i2
	or	%i3,%i2,%i2
	ld	[%g3+3*8+0],%i4
	ld	[%g3+3*8+4],%i3
	sllx	%i3,32,%i3
	or	%i4,%i3,%i3
	ld	[%g3+4*8+0],%i5
	ld	[%g3+4*8+4],%i4
	sllx	%i4,32,%i4
	or	%i5,%i4,%i4
	ld	[%g3+5*8+0],%l0
	ld	[%g3+5*8+4],%i5
	sllx	%i5,32,%i5
	or	%l0,%i5,%i5
	ld	[%g3+6*8+0],%l1
	ld	[%g3+6*8+4],%l0
	sllx	%l0,32,%l0
	or	%l1,%l0,%l0
	ld	[%g3+7*8+0],%l2
	ld	[%g3+7*8+4],%l1
	sllx	%l1,32,%l1
	or	%l2,%l1,%l1
	ld	[%g3+8*8+0],%l3
	ld	[%g3+8*8+4],%l2
	sllx	%l2,32,%l2
	or	%l3,%l2,%l2
	ld	[%g3+9*8+0],%l4
	ld	[%g3+9*8+4],%l3
	sllx	%l3,32,%l3
	or	%l4,%l3,%l3
	ld	[%g3+10*8+0],%l5
	ld	[%g3+10*8+4],%l4
	sllx	%l4,32,%l4
	or	%l5,%l4,%l4
	ld	[%g3+11*8+0],%l6
	ld	[%g3+11*8+4],%l5
	sllx	%l5,32,%l5
	or	%l6,%l5,%l5
	ld	[%g3+12*8+0],%l7
	ld	[%g3+12*8+4],%l6
	sllx	%l6,32,%l6
	or	%l7,%l6,%l6
	ld	[%g3+13*8+0],%o7
	ld	[%g3+13*8+4],%l7
	sllx	%l7,32,%l7
	or	%o7,%l7,%l7
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	ld	[%g3+14*8+0],%i1
	ld	[%g3+14*8+4],%i0
	sllx	%i0,32,%i0
	or	%i1,%i0,%i0
	ld	[%g3+15*8+0],%i2
	ld	[%g3+15*8+4],%i1
	sllx	%i1,32,%i1
	or	%i2,%i1,%i1
	ld	[%g3+16*8+0],%i3
	ld	[%g3+16*8+4],%i2
	sllx	%i2,32,%i2
	or	%i3,%i2,%i2
	ld	[%g3+17*8+0],%i4
	ld	[%g3+17*8+4],%i3
	sllx	%i3,32,%i3
	or	%i4,%i3,%i3
	ld	[%g3+18*8+0],%i5
	ld	[%g3+18*8+4],%i4
	sllx	%i4,32,%i4
	or	%i5,%i4,%i4
	ld	[%g3+19*8+0],%l0
	ld	[%g3+19*8+4],%i5
	sllx	%i5,32,%i5
	or	%l0,%i5,%i5
	ld	[%g3+20*8+0],%l1
	ld	[%g3+20*8+4],%l0
	sllx	%l0,32,%l0
	or	%l1,%l0,%l0
	ld	[%g3+21*8+0],%l2
	ld	[%g3+21*8+4],%l1
	sllx	%l1,32,%l1
	or	%l2,%l1,%l1
	ld	[%g3+22*8+0],%l3
	ld	[%g3+22*8+4],%l2
	sllx	%l2,32,%l2
	or	%l3,%l2,%l2
	ld	[%g3+23*8+0],%o7
	ld	[%g3+23*8+4],%l3
	sllx	%l3,32,%l3
	or	%o7,%l3,%l3
	.word	0x81b02920+24-1	! montmul	24-1
.Lmresume_24:
	fbu,pn	%fcc3,.Lmabort_24
#ifndef	__arch64__
	and	%fp,%g5,%g5
	brz,pn	%g5,.Lmabort_24
#endif
	nop
#ifdef	__arch64__
	restore
	restore
	restore
	restore
	restore
#else
	restore;		and	%fp,%g5,%g5
	restore;		and	%fp,%g5,%g5
	restore;		and	%fp,%g5,%g5
	restore;		and	%fp,%g5,%g5
	 brz,pn	%g5,.Lmabort1_24
	restore
#endif
	.word	0x81b02310 !movxtod	%l0,%f0
	.word	0x85b02311 !movxtod	%l1,%f2
	.word	0x89b02312 !movxtod	%l2,%f4
	.word	0x8db02313 !movxtod	%l3,%f6
	.word	0x91b02314 !movxtod	%l4,%f8
	.word	0x95b02315 !movxtod	%l5,%f10
	.word	0x99b02316 !movxtod	%l6,%f12
	.word	0x9db02317 !movxtod	%l7,%f14
	.word	0xa1b02308 !movxtod	%o0,%f16
	.word	0xa5b02309 !movxtod	%o1,%f18
	.word	0xa9b0230a !movxtod	%o2,%f20
	.word	0xadb0230b !movxtod	%o3,%f22
	.word	0xbbb0230c !movxtod	%o4,%f60
	.word	0xbfb0230d !movxtod	%o5,%f62
#ifdef	__arch64__
	restore
#else
	 and	%fp,%g5,%g5
	restore
	 and	%g5,1,%o7
	 and	%fp,%g5,%g5
	 srl	%fp,0,%fp		! just in case?
	 or	%o7,%g5,%g5
	brz,a,pn %g5,.Lmdone_24
	mov	0,%i0		! return failure
#endif
	st	%f1,[%g1+0*8+0]
	st	%f0,[%g1+0*8+4]
	st	%f3,[%g1+1*8+0]
	st	%f2,[%g1+1*8+4]
	st	%f5,[%g1+2*8+0]
	st	%f4,[%g1+2*8+4]
	st	%f7,[%g1+3*8+0]
	st	%f6,[%g1+3*8+4]
	st	%f9,[%g1+4*8+0]
	st	%f8,[%g1+4*8+4]
	st	%f11,[%g1+5*8+0]
	st	%f10,[%g1+5*8+4]
	st	%f13,[%g1+6*8+0]
	st	%f12,[%g1+6*8+4]
	st	%f15,[%g1+7*8+0]
	st	%f14,[%g1+7*8+4]
	st	%f17,[%g1+8*8+0]
	st	%f16,[%g1+8*8+4]
	st	%f19,[%g1+9*8+0]
	st	%f18,[%g1+9*8+4]
	st	%f21,[%g1+10*8+0]
	st	%f20,[%g1+10*8+4]
	st	%f23,[%g1+11*8+0]
	st	%f22,[%g1+11*8+4]
	.word	0x81b00f1d !fsrc2	%f0,%f60,%f0
	st	%f1,[%g1+12*8+0]
	st	%f0,[%g1+12*8+4]
	.word	0x85b00f1f !fsrc2	%f0,%f62,%f2
	st	%f3,[%g1+13*8+0]
	st	%f2,[%g1+13*8+4]
	.word	0x89b00f18 !fsrc2	%f0,%f24,%f4
	st	%f5,[%g1+14*8+0]
	st	%f4,[%g1+14*8+4]
	.word	0x8db00f1a !fsrc2	%f0,%f26,%f6
	st	%f7,[%g1+15*8+0]
	st	%f6,[%g1+15*8+4]
	.word	0x81b00f1c !fsrc2	%f0,%f28,%f0
	st	%f1,[%g1+16*8+0]
	st	%f0,[%g1+16*8+4]
	.word	0x85b00f1e !fsrc2	%f0,%f30,%f2
	st	%f3,[%g1+17*8+0]
	st	%f2,[%g1+17*8+4]
	.word	0x89b00f01 !fsrc2	%f0,%f32,%f4
	st	%f5,[%g1+18*8+0]
	st	%f4,[%g1+18*8+4]
	.word	0x8db00f03 !fsrc2	%f0,%f34,%f6
	st	%f7,[%g1+19*8+0]
	st	%f6,[%g1+19*8+4]
	.word	0x81b00f05 !fsrc2	%f0,%f36,%f0
	st	%f1,[%g1+20*8+0]
	st	%f0,[%g1+20*8+4]
	.word	0x85b00f07 !fsrc2	%f0,%f38,%f2
	st	%f3,[%g1+21*8+0]
	st	%f2,[%g1+21*8+4]
	.word	0x89b00f09 !fsrc2	%f0,%f40,%f4
	st	%f5,[%g1+22*8+0]
	st	%f4,[%g1+22*8+4]
	.word	0x8db00f0b !fsrc2	%f0,%f42,%f6
	st	%f7,[%g1+23*8+0]
	st	%f6,[%g1+23*8+4]
	mov	1,%i0		! return success
.Lmdone_24:
	ret
	restore

.Lmabort_24:
	restore
	restore
	restore
	restore
	restore
.Lmabort1_24:
	restore

	mov	0,%i0		! return failure
	ret
	restore

.align	32
.Lmsquare_24:
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	.word   0x81b02940+24-1	! montsqr	24-1
	ba	.Lmresume_24
	nop
.type	bn_mul_mont_t4_24, #function
.size	bn_mul_mont_t4_24, .-bn_mul_mont_t4_24
.globl	bn_mul_mont_t4_32
.align	32
bn_mul_mont_t4_32:
#ifdef	__arch64__
	mov	0,%g5
	mov	-128,%g4
#elif defined(SPARCV9_64BIT_STACK)
	SPARC_LOAD_ADDRESS_LEAF(OPENSSL_sparcv9cap_P,%g1,%g5)
	ld	[%g1+0],%g1	! OPENSSL_sparcv9_P[0]
	mov	-2047,%g4
	and	%g1,SPARCV9_64BIT_STACK,%g1
	movrz	%g1,0,%g4
	mov	-1,%g5
	add	%g4,-128,%g4
#else
	mov	-1,%g5
	mov	-128,%g4
#endif
	sllx	%g5,32,%g5
	save	%sp,%g4,%sp
#ifndef	__arch64__
	save	%sp,-128,%sp	! warm it up
	save	%sp,-128,%sp
	save	%sp,-128,%sp
	save	%sp,-128,%sp
	save	%sp,-128,%sp
	save	%sp,-128,%sp
	restore
	restore
	restore
	restore
	restore
	restore
#endif
	and	%sp,1,%g4
	or	%g5,%fp,%fp
	or	%g4,%g5,%g5

	! copy arguments to global registers
	mov	%i0,%g1
	mov	%i1,%g2
	mov	%i2,%g3
	mov	%i3,%g4
	ld	[%i4+0],%f1	! load *n0
	ld	[%i4+4],%f0
	.word	0xbbb00f00 !fsrc2	%f0,%f0,%f60
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	ld	[%g2+0*8+0],%l1
	ld	[%g2+0*8+4],%l0
	sllx	%l0,32,%l0
	or	%l1,%l0,%l0
	ld	[%g2+1*8+0],%l2
	ld	[%g2+1*8+4],%l1
	sllx	%l1,32,%l1
	or	%l2,%l1,%l1
	ld	[%g2+2*8+0],%l3
	ld	[%g2+2*8+4],%l2
	sllx	%l2,32,%l2
	or	%l3,%l2,%l2
	ld	[%g2+3*8+0],%l4
	ld	[%g2+3*8+4],%l3
	sllx	%l3,32,%l3
	or	%l4,%l3,%l3
	ld	[%g2+4*8+0],%l5
	ld	[%g2+4*8+4],%l4
	sllx	%l4,32,%l4
	or	%l5,%l4,%l4
	ld	[%g2+5*8+0],%l6
	ld	[%g2+5*8+4],%l5
	sllx	%l5,32,%l5
	or	%l6,%l5,%l5
	ld	[%g2+6*8+0],%l7
	ld	[%g2+6*8+4],%l6
	sllx	%l6,32,%l6
	or	%l7,%l6,%l6
	ld	[%g2+7*8+0],%o0
	ld	[%g2+7*8+4],%l7
	sllx	%l7,32,%l7
	or	%o0,%l7,%l7
	ld	[%g2+8*8+0],%o1
	ld	[%g2+8*8+4],%o0
	sllx	%o0,32,%o0
	or	%o1,%o0,%o0
	ld	[%g2+9*8+0],%o2
	ld	[%g2+9*8+4],%o1
	sllx	%o1,32,%o1
	or	%o2,%o1,%o1
	ld	[%g2+10*8+0],%o3
	ld	[%g2+10*8+4],%o2
	sllx	%o2,32,%o2
	or	%o3,%o2,%o2
	ld	[%g2+11*8+0],%o4
	ld	[%g2+11*8+4],%o3
	sllx	%o3,32,%o3
	or	%o4,%o3,%o3
	ld	[%g2+12*8+0],%o5
	ld	[%g2+12*8+4],%o4
	sllx	%o4,32,%o4
	or	%o5,%o4,%o4
	ld	[%g2+13*8+0],%o7
	ld	[%g2+13*8+4],%o5
	sllx	%o5,32,%o5
	or	%o7,%o5,%o5
	ld	[%g2+14*8+0],%f5
	ld	[%g2+14*8+4],%f4
	.word	0xb1b00f04 !fsrc2	%f0,%f4,%f24
	ld	[%g2+15*8+0],%f7
	ld	[%g2+15*8+4],%f6
	.word	0xb5b00f06 !fsrc2	%f0,%f6,%f26
	ld	[%g2+16*8+0],%f1
	ld	[%g2+16*8+4],%f0
	.word	0xb9b00f00 !fsrc2	%f0,%f0,%f28
	ld	[%g2+17*8+0],%f3
	ld	[%g2+17*8+4],%f2
	.word	0xbdb00f02 !fsrc2	%f0,%f2,%f30
	ld	[%g2+18*8+0],%f5
	ld	[%g2+18*8+4],%f4
	.word	0x83b00f04 !fsrc2	%f0,%f4,%f32
	ld	[%g2+19*8+0],%f7
	ld	[%g2+19*8+4],%f6
	.word	0x87b00f06 !fsrc2	%f0,%f6,%f34
	ld	[%g2+20*8+0],%f1
	ld	[%g2+20*8+4],%f0
	.word	0x8bb00f00 !fsrc2	%f0,%f0,%f36
	ld	[%g2+21*8+0],%f3
	ld	[%g2+21*8+4],%f2
	.word	0x8fb00f02 !fsrc2	%f0,%f2,%f38
	ld	[%g2+22*8+0],%f5
	ld	[%g2+22*8+4],%f4
	.word	0x93b00f04 !fsrc2	%f0,%f4,%f40
	ld	[%g2+23*8+0],%f7
	ld	[%g2+23*8+4],%f6
	.word	0x97b00f06 !fsrc2	%f0,%f6,%f42
	ld	[%g2+24*8+0],%f1
	ld	[%g2+24*8+4],%f0
	.word	0x9bb00f00 !fsrc2	%f0,%f0,%f44
	ld	[%g2+25*8+0],%f3
	ld	[%g2+25*8+4],%f2
	.word	0x9fb00f02 !fsrc2	%f0,%f2,%f46
	ld	[%g2+26*8+0],%f5
	ld	[%g2+26*8+4],%f4
	.word	0xa3b00f04 !fsrc2	%f0,%f4,%f48
	ld	[%g2+27*8+0],%f7
	ld	[%g2+27*8+4],%f6
	.word	0xa7b00f06 !fsrc2	%f0,%f6,%f50
	ld	[%g2+28*8+0],%f1
	ld	[%g2+28*8+4],%f0
	.word	0xabb00f00 !fsrc2	%f0,%f0,%f52
	ld	[%g2+29*8+0],%f3
	ld	[%g2+29*8+4],%f2
	.word	0xafb00f02 !fsrc2	%f0,%f2,%f54
	ld	[%g2+30*8+0],%f5
	ld	[%g2+30*8+4],%f4
	.word	0xb3b00f04 !fsrc2	%f0,%f4,%f56
	ld	[%g2+31*8+0],%f7
	ld	[%g2+31*8+4],%f6
	.word	0xb7b00f06 !fsrc2	%f0,%f6,%f58
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	ld	[%g4+0*8+0],%l1
	ld	[%g4+0*8+4],%l0
	sllx	%l0,32,%l0
	or	%l1,%l0,%l0
	ld	[%g4+1*8+0],%l2
	ld	[%g4+1*8+4],%l1
	sllx	%l1,32,%l1
	or	%l2,%l1,%l1
	ld	[%g4+2*8+0],%l3
	ld	[%g4+2*8+4],%l2
	sllx	%l2,32,%l2
	or	%l3,%l2,%l2
	ld	[%g4+3*8+0],%l4
	ld	[%g4+3*8+4],%l3
	sllx	%l3,32,%l3
	or	%l4,%l3,%l3
	ld	[%g4+4*8+0],%l5
	ld	[%g4+4*8+4],%l4
	sllx	%l4,32,%l4
	or	%l5,%l4,%l4
	ld	[%g4+5*8+0],%l6
	ld	[%g4+5*8+4],%l5
	sllx	%l5,32,%l5
	or	%l6,%l5,%l5
	ld	[%g4+6*8+0],%l7
	ld	[%g4+6*8+4],%l6
	sllx	%l6,32,%l6
	or	%l7,%l6,%l6
	ld	[%g4+7*8+0],%o0
	ld	[%g4+7*8+4],%l7
	sllx	%l7,32,%l7
	or	%o0,%l7,%l7
	ld	[%g4+8*8+0],%o1
	ld	[%g4+8*8+4],%o0
	sllx	%o0,32,%o0
	or	%o1,%o0,%o0
	ld	[%g4+9*8+0],%o2
	ld	[%g4+9*8+4],%o1
	sllx	%o1,32,%o1
	or	%o2,%o1,%o1
	ld	[%g4+10*8+0],%o3
	ld	[%g4+10*8+4],%o2
	sllx	%o2,32,%o2
	or	%o3,%o2,%o2
	ld	[%g4+11*8+0],%o4
	ld	[%g4+11*8+4],%o3
	sllx	%o3,32,%o3
	or	%o4,%o3,%o3
	ld	[%g4+12*8+0],%o5
	ld	[%g4+12*8+4],%o4
	sllx	%o4,32,%o4
	or	%o5,%o4,%o4
	ld	[%g4+13*8+0],%o7
	ld	[%g4+13*8+4],%o5
	sllx	%o5,32,%o5
	or	%o7,%o5,%o5
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	ld	[%g4+14*8+0],%l1
	ld	[%g4+14*8+4],%l0
	sllx	%l0,32,%l0
	or	%l1,%l0,%l0
	ld	[%g4+15*8+0],%l2
	ld	[%g4+15*8+4],%l1
	sllx	%l1,32,%l1
	or	%l2,%l1,%l1
	ld	[%g4+16*8+0],%l3
	ld	[%g4+16*8+4],%l2
	sllx	%l2,32,%l2
	or	%l3,%l2,%l2
	ld	[%g4+17*8+0],%l4
	ld	[%g4+17*8+4],%l3
	sllx	%l3,32,%l3
	or	%l4,%l3,%l3
	ld	[%g4+18*8+0],%l5
	ld	[%g4+18*8+4],%l4
	sllx	%l4,32,%l4
	or	%l5,%l4,%l4
	ld	[%g4+19*8+0],%l6
	ld	[%g4+19*8+4],%l5
	sllx	%l5,32,%l5
	or	%l6,%l5,%l5
	ld	[%g4+20*8+0],%l7
	ld	[%g4+20*8+4],%l6
	sllx	%l6,32,%l6
	or	%l7,%l6,%l6
	ld	[%g4+21*8+0],%o0
	ld	[%g4+21*8+4],%l7
	sllx	%l7,32,%l7
	or	%o0,%l7,%l7
	ld	[%g4+22*8+0],%o1
	ld	[%g4+22*8+4],%o0
	sllx	%o0,32,%o0
	or	%o1,%o0,%o0
	ld	[%g4+23*8+0],%o2
	ld	[%g4+23*8+4],%o1
	sllx	%o1,32,%o1
	or	%o2,%o1,%o1
	ld	[%g4+24*8+0],%o3
	ld	[%g4+24*8+4],%o2
	sllx	%o2,32,%o2
	or	%o3,%o2,%o2
	ld	[%g4+25*8+0],%o4
	ld	[%g4+25*8+4],%o3
	sllx	%o3,32,%o3
	or	%o4,%o3,%o3
	ld	[%g4+26*8+0],%o5
	ld	[%g4+26*8+4],%o4
	sllx	%o4,32,%o4
	or	%o5,%o4,%o4
	ld	[%g4+27*8+0],%o7
	ld	[%g4+27*8+4],%o5
	sllx	%o5,32,%o5
	or	%o7,%o5,%o5
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	ld	[%g4+28*8+0],%l1
	ld	[%g4+28*8+4],%l0
	sllx	%l0,32,%l0
	or	%l1,%l0,%l0
	ld	[%g4+29*8+0],%l2
	ld	[%g4+29*8+4],%l1
	sllx	%l1,32,%l1
	or	%l2,%l1,%l1
	ld	[%g4+30*8+0],%l3
	ld	[%g4+30*8+4],%l2
	sllx	%l2,32,%l2
	or	%l3,%l2,%l2
	ld	[%g4+31*8+0],%o7
	ld	[%g4+31*8+4],%l3
	sllx	%l3,32,%l3
	or	%o7,%l3,%l3
	cmp	%g2,%g3
	be	SIZE_T_CC,.Lmsquare_32
	nop
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	ld	[%g3+0*8+0],%i1
	ld	[%g3+0*8+4],%i0
	sllx	%i0,32,%i0
	or	%i1,%i0,%i0
	ld	[%g3+1*8+0],%i2
	ld	[%g3+1*8+4],%i1
	sllx	%i1,32,%i1
	or	%i2,%i1,%i1
	ld	[%g3+2*8+0],%i3
	ld	[%g3+2*8+4],%i2
	sllx	%i2,32,%i2
	or	%i3,%i2,%i2
	ld	[%g3+3*8+0],%i4
	ld	[%g3+3*8+4],%i3
	sllx	%i3,32,%i3
	or	%i4,%i3,%i3
	ld	[%g3+4*8+0],%i5
	ld	[%g3+4*8+4],%i4
	sllx	%i4,32,%i4
	or	%i5,%i4,%i4
	ld	[%g3+5*8+0],%l0
	ld	[%g3+5*8+4],%i5
	sllx	%i5,32,%i5
	or	%l0,%i5,%i5
	ld	[%g3+6*8+0],%l1
	ld	[%g3+6*8+4],%l0
	sllx	%l0,32,%l0
	or	%l1,%l0,%l0
	ld	[%g3+7*8+0],%l2
	ld	[%g3+7*8+4],%l1
	sllx	%l1,32,%l1
	or	%l2,%l1,%l1
	ld	[%g3+8*8+0],%l3
	ld	[%g3+8*8+4],%l2
	sllx	%l2,32,%l2
	or	%l3,%l2,%l2
	ld	[%g3+9*8+0],%l4
	ld	[%g3+9*8+4],%l3
	sllx	%l3,32,%l3
	or	%l4,%l3,%l3
	ld	[%g3+10*8+0],%l5
	ld	[%g3+10*8+4],%l4
	sllx	%l4,32,%l4
	or	%l5,%l4,%l4
	ld	[%g3+11*8+0],%l6
	ld	[%g3+11*8+4],%l5
	sllx	%l5,32,%l5
	or	%l6,%l5,%l5
	ld	[%g3+12*8+0],%l7
	ld	[%g3+12*8+4],%l6
	sllx	%l6,32,%l6
	or	%l7,%l6,%l6
	ld	[%g3+13*8+0],%o7
	ld	[%g3+13*8+4],%l7
	sllx	%l7,32,%l7
	or	%o7,%l7,%l7
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	ld	[%g3+14*8+0],%i1
	ld	[%g3+14*8+4],%i0
	sllx	%i0,32,%i0
	or	%i1,%i0,%i0
	ld	[%g3+15*8+0],%i2
	ld	[%g3+15*8+4],%i1
	sllx	%i1,32,%i1
	or	%i2,%i1,%i1
	ld	[%g3+16*8+0],%i3
	ld	[%g3+16*8+4],%i2
	sllx	%i2,32,%i2
	or	%i3,%i2,%i2
	ld	[%g3+17*8+0],%i4
	ld	[%g3+17*8+4],%i3
	sllx	%i3,32,%i3
	or	%i4,%i3,%i3
	ld	[%g3+18*8+0],%i5
	ld	[%g3+18*8+4],%i4
	sllx	%i4,32,%i4
	or	%i5,%i4,%i4
	ld	[%g3+19*8+0],%l0
	ld	[%g3+19*8+4],%i5
	sllx	%i5,32,%i5
	or	%l0,%i5,%i5
	ld	[%g3+20*8+0],%l1
	ld	[%g3+20*8+4],%l0
	sllx	%l0,32,%l0
	or	%l1,%l0,%l0
	ld	[%g3+21*8+0],%l2
	ld	[%g3+21*8+4],%l1
	sllx	%l1,32,%l1
	or	%l2,%l1,%l1
	ld	[%g3+22*8+0],%l3
	ld	[%g3+22*8+4],%l2
	sllx	%l2,32,%l2
	or	%l3,%l2,%l2
	ld	[%g3+23*8+0],%l4
	ld	[%g3+23*8+4],%l3
	sllx	%l3,32,%l3
	or	%l4,%l3,%l3
	ld	[%g3+24*8+0],%l5
	ld	[%g3+24*8+4],%l4
	sllx	%l4,32,%l4
	or	%l5,%l4,%l4
	ld	[%g3+25*8+0],%l6
	ld	[%g3+25*8+4],%l5
	sllx	%l5,32,%l5
	or	%l6,%l5,%l5
	ld	[%g3+26*8+0],%l7
	ld	[%g3+26*8+4],%l6
	sllx	%l6,32,%l6
	or	%l7,%l6,%l6
	ld	[%g3+27*8+0],%o0
	ld	[%g3+27*8+4],%l7
	sllx	%l7,32,%l7
	or	%o0,%l7,%l7
	ld	[%g3+28*8+0],%o1
	ld	[%g3+28*8+4],%o0
	sllx	%o0,32,%o0
	or	%o1,%o0,%o0
	ld	[%g3+29*8+0],%o2
	ld	[%g3+29*8+4],%o1
	sllx	%o1,32,%o1
	or	%o2,%o1,%o1
	ld	[%g3+30*8+0],%o3
	ld	[%g3+30*8+4],%o2
	sllx	%o2,32,%o2
	or	%o3,%o2,%o2
	ld	[%g3+31*8+0],%o7
	ld	[%g3+31*8+4],%o3
	sllx	%o3,32,%o3
	or	%o7,%o3,%o3
	.word	0x81b02920+32-1	! montmul	32-1
.Lmresume_32:
	fbu,pn	%fcc3,.Lmabort_32
#ifndef	__arch64__
	and	%fp,%g5,%g5
	brz,pn	%g5,.Lmabort_32
#endif
	nop
#ifdef	__arch64__
	restore
	restore
	restore
	restore
	restore
#else
	restore;		and	%fp,%g5,%g5
	restore;		and	%fp,%g5,%g5
	restore;		and	%fp,%g5,%g5
	restore;		and	%fp,%g5,%g5
	 brz,pn	%g5,.Lmabort1_32
	restore
#endif
	.word	0x81b02310 !movxtod	%l0,%f0
	.word	0x85b02311 !movxtod	%l1,%f2
	.word	0x89b02312 !movxtod	%l2,%f4
	.word	0x8db02313 !movxtod	%l3,%f6
	.word	0x91b02314 !movxtod	%l4,%f8
	.word	0x95b02315 !movxtod	%l5,%f10
	.word	0x99b02316 !movxtod	%l6,%f12
	.word	0x9db02317 !movxtod	%l7,%f14
	.word	0xa1b02308 !movxtod	%o0,%f16
	.word	0xa5b02309 !movxtod	%o1,%f18
	.word	0xa9b0230a !movxtod	%o2,%f20
	.word	0xadb0230b !movxtod	%o3,%f22
	.word	0xbbb0230c !movxtod	%o4,%f60
	.word	0xbfb0230d !movxtod	%o5,%f62
#ifdef	__arch64__
	restore
#else
	 and	%fp,%g5,%g5
	restore
	 and	%g5,1,%o7
	 and	%fp,%g5,%g5
	 srl	%fp,0,%fp		! just in case?
	 or	%o7,%g5,%g5
	brz,a,pn %g5,.Lmdone_32
	mov	0,%i0		! return failure
#endif
	st	%f1,[%g1+0*8+0]
	st	%f0,[%g1+0*8+4]
	st	%f3,[%g1+1*8+0]
	st	%f2,[%g1+1*8+4]
	st	%f5,[%g1+2*8+0]
	st	%f4,[%g1+2*8+4]
	st	%f7,[%g1+3*8+0]
	st	%f6,[%g1+3*8+4]
	st	%f9,[%g1+4*8+0]
	st	%f8,[%g1+4*8+4]
	st	%f11,[%g1+5*8+0]
	st	%f10,[%g1+5*8+4]
	st	%f13,[%g1+6*8+0]
	st	%f12,[%g1+6*8+4]
	st	%f15,[%g1+7*8+0]
	st	%f14,[%g1+7*8+4]
	st	%f17,[%g1+8*8+0]
	st	%f16,[%g1+8*8+4]
	st	%f19,[%g1+9*8+0]
	st	%f18,[%g1+9*8+4]
	st	%f21,[%g1+10*8+0]
	st	%f20,[%g1+10*8+4]
	st	%f23,[%g1+11*8+0]
	st	%f22,[%g1+11*8+4]
	.word	0x81b00f1d !fsrc2	%f0,%f60,%f0
	st	%f1,[%g1+12*8+0]
	st	%f0,[%g1+12*8+4]
	.word	0x85b00f1f !fsrc2	%f0,%f62,%f2
	st	%f3,[%g1+13*8+0]
	st	%f2,[%g1+13*8+4]
	.word	0x89b00f18 !fsrc2	%f0,%f24,%f4
	st	%f5,[%g1+14*8+0]
	st	%f4,[%g1+14*8+4]
	.word	0x8db00f1a !fsrc2	%f0,%f26,%f6
	st	%f7,[%g1+15*8+0]
	st	%f6,[%g1+15*8+4]
	.word	0x81b00f1c !fsrc2	%f0,%f28,%f0
	st	%f1,[%g1+16*8+0]
	st	%f0,[%g1+16*8+4]
	.word	0x85b00f1e !fsrc2	%f0,%f30,%f2
	st	%f3,[%g1+17*8+0]
	st	%f2,[%g1+17*8+4]
	.word	0x89b00f01 !fsrc2	%f0,%f32,%f4
	st	%f5,[%g1+18*8+0]
	st	%f4,[%g1+18*8+4]
	.word	0x8db00f03 !fsrc2	%f0,%f34,%f6
	st	%f7,[%g1+19*8+0]
	st	%f6,[%g1+19*8+4]
	.word	0x81b00f05 !fsrc2	%f0,%f36,%f0
	st	%f1,[%g1+20*8+0]
	st	%f0,[%g1+20*8+4]
	.word	0x85b00f07 !fsrc2	%f0,%f38,%f2
	st	%f3,[%g1+21*8+0]
	st	%f2,[%g1+21*8+4]
	.word	0x89b00f09 !fsrc2	%f0,%f40,%f4
	st	%f5,[%g1+22*8+0]
	st	%f4,[%g1+22*8+4]
	.word	0x8db00f0b !fsrc2	%f0,%f42,%f6
	st	%f7,[%g1+23*8+0]
	st	%f6,[%g1+23*8+4]
	.word	0x81b00f0d !fsrc2	%f0,%f44,%f0
	st	%f1,[%g1+24*8+0]
	st	%f0,[%g1+24*8+4]
	.word	0x85b00f0f !fsrc2	%f0,%f46,%f2
	st	%f3,[%g1+25*8+0]
	st	%f2,[%g1+25*8+4]
	.word	0x89b00f11 !fsrc2	%f0,%f48,%f4
	st	%f5,[%g1+26*8+0]
	st	%f4,[%g1+26*8+4]
	.word	0x8db00f13 !fsrc2	%f0,%f50,%f6
	st	%f7,[%g1+27*8+0]
	st	%f6,[%g1+27*8+4]
	.word	0x81b00f15 !fsrc2	%f0,%f52,%f0
	st	%f1,[%g1+28*8+0]
	st	%f0,[%g1+28*8+4]
	.word	0x85b00f17 !fsrc2	%f0,%f54,%f2
	st	%f3,[%g1+29*8+0]
	st	%f2,[%g1+29*8+4]
	.word	0x89b00f19 !fsrc2	%f0,%f56,%f4
	st	%f5,[%g1+30*8+0]
	st	%f4,[%g1+30*8+4]
	.word	0x8db00f1b !fsrc2	%f0,%f58,%f6
	st	%f7,[%g1+31*8+0]
	st	%f6,[%g1+31*8+4]
	mov	1,%i0		! return success
.Lmdone_32:
	ret
	restore

.Lmabort_32:
	restore
	restore
	restore
	restore
	restore
.Lmabort1_32:
	restore

	mov	0,%i0		! return failure
	ret
	restore

.align	32
.Lmsquare_32:
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	.word   0x81b02940+32-1	! montsqr	32-1
	ba	.Lmresume_32
	nop
.type	bn_mul_mont_t4_32, #function
.size	bn_mul_mont_t4_32, .-bn_mul_mont_t4_32
.globl	bn_pwr5_mont_t4_8
.align	32
bn_pwr5_mont_t4_8:
#ifdef	__arch64__
	mov	0,%g5
	mov	-128,%g4
#elif defined(SPARCV9_64BIT_STACK)
	SPARC_LOAD_ADDRESS_LEAF(OPENSSL_sparcv9cap_P,%g1,%g5)
	ld	[%g1+0],%g1	! OPENSSL_sparcv9_P[0]
	mov	-2047,%g4
	and	%g1,SPARCV9_64BIT_STACK,%g1
	movrz	%g1,0,%g4
	mov	-1,%g5
	add	%g4,-128,%g4
#else
	mov	-1,%g5
	mov	-128,%g4
#endif
	sllx	%g5,32,%g5
	save	%sp,%g4,%sp
#ifndef	__arch64__
	save	%sp,-128,%sp	! warm it up
	save	%sp,-128,%sp
	save	%sp,-128,%sp
	save	%sp,-128,%sp
	save	%sp,-128,%sp
	save	%sp,-128,%sp
	restore
	restore
	restore
	restore
	restore
	restore
#endif
	and	%sp,1,%g4
	or	%g5,%fp,%fp
	or	%g4,%g5,%g5

	! copy arguments to global registers
	mov	%i0,%g1
	mov	%i1,%g2
	ld	[%i2+0],%f1	! load *n0
	ld	[%i2+4],%f0
	mov	%i3,%g3
	srl	%i4,%g0,%i4	! pack last arguments
	sllx	%i5,32,%g4
	or	%i4,%g4,%g4
	.word	0xbbb00f00 !fsrc2	%f0,%f0,%f60
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	ldx	[%g1+0*8],%l0
	ldx	[%g1+1*8],%l1
	ldx	[%g1+2*8],%l2
	ldx	[%g1+3*8],%l3
	ldx	[%g1+4*8],%l4
	ldx	[%g1+5*8],%l5
	ldx	[%g1+6*8],%l6
	ldx	[%g1+7*8],%l7
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	ldx	[%g2+0*8],%l0
	ldx	[%g2+1*8],%l1
	ldx	[%g2+2*8],%l2
	ldx	[%g2+3*8],%l3
	ldx	[%g2+4*8],%l4
	ldx	[%g2+5*8],%l5
	ldx	[%g2+6*8],%l6
	ldx	[%g2+7*8],%l7
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	save	%sp,-128,%sp;		or	%g5,%fp,%fp

	srlx	%g4,	32,	%o4		! unpack %g4
	srl	%g4,	%g0,	%o5
	sub	%o4,	5,	%o4
	mov	%g3,	%o7
	sllx	%o4,	32,	%g4		! re-pack %g4
	or	%o5,	%g4,	%g4
	srl	%o5,	%o4,	%o5
	srl	%o5,	2,	%o4
	and	%o5,	3,	%o5
	and	%o4,	7,	%o4
	sll	%o5,	3,	%o5	! offset within first cache line
	add	%o5,	%o7,	%o7	! of the pwrtbl
	or	%g0,	1,	%o5
	sll	%o5,	%o4,	%o4
	wr	%o4,	%g0,	%ccr
	b	.Lstride_8
	nop
.align	16
.Lstride_8:
	ldx	[%o7+0*32],	%i0
	ldx	[%o7+8*32],	%i1
	ldx	[%o7+1*32],	%o4
	ldx	[%o7+9*32],	%o5
	movvs	%icc,	%o4,	%i0
	ldx	[%o7+2*32],	%o4
	movvs	%icc,	%o5,	%i1
	ldx	[%o7+10*32],%o5
	move	%icc,	%o4,	%i0
	ldx	[%o7+3*32],	%o4
	move	%icc,	%o5,	%i1
	ldx	[%o7+11*32],%o5
	movneg	%icc,	%o4,	%i0
	ldx	[%o7+4*32],	%o4
	movneg	%icc,	%o5,	%i1
	ldx	[%o7+12*32],%o5
	movcs	%xcc,	%o4,	%i0
	ldx	[%o7+5*32],%o4
	movcs	%xcc,	%o5,	%i1
	ldx	[%o7+13*32],%o5
	movvs	%xcc,	%o4,	%i0
	ldx	[%o7+6*32],	%o4
	movvs	%xcc,	%o5,	%i1
	ldx	[%o7+14*32],%o5
	move	%xcc,	%o4,	%i0
	ldx	[%o7+7*32],	%o4
	move	%xcc,	%o5,	%i1
	ldx	[%o7+15*32],%o5
	movneg	%xcc,	%o4,	%i0
	add	%o7,16*32,	%o7
	movneg	%xcc,	%o5,	%i1
	ldx	[%o7+0*32],	%i2
	ldx	[%o7+8*32],	%i3
	ldx	[%o7+1*32],	%o4
	ldx	[%o7+9*32],	%o5
	movvs	%icc,	%o4,	%i2
	ldx	[%o7+2*32],	%o4
	movvs	%icc,	%o5,	%i3
	ldx	[%o7+10*32],%o5
	move	%icc,	%o4,	%i2
	ldx	[%o7+3*32],	%o4
	move	%icc,	%o5,	%i3
	ldx	[%o7+11*32],%o5
	movneg	%icc,	%o4,	%i2
	ldx	[%o7+4*32],	%o4
	movneg	%icc,	%o5,	%i3
	ldx	[%o7+12*32],%o5
	movcs	%xcc,	%o4,	%i2
	ldx	[%o7+5*32],%o4
	movcs	%xcc,	%o5,	%i3
	ldx	[%o7+13*32],%o5
	movvs	%xcc,	%o4,	%i2
	ldx	[%o7+6*32],	%o4
	movvs	%xcc,	%o5,	%i3
	ldx	[%o7+14*32],%o5
	move	%xcc,	%o4,	%i2
	ldx	[%o7+7*32],	%o4
	move	%xcc,	%o5,	%i3
	ldx	[%o7+15*32],%o5
	movneg	%xcc,	%o4,	%i2
	add	%o7,16*32,	%o7
	movneg	%xcc,	%o5,	%i3
	ldx	[%o7+0*32],	%i4
	ldx	[%o7+8*32],	%i5
	ldx	[%o7+1*32],	%o4
	ldx	[%o7+9*32],	%o5
	movvs	%icc,	%o4,	%i4
	ldx	[%o7+2*32],	%o4
	movvs	%icc,	%o5,	%i5
	ldx	[%o7+10*32],%o5
	move	%icc,	%o4,	%i4
	ldx	[%o7+3*32],	%o4
	move	%icc,	%o5,	%i5
	ldx	[%o7+11*32],%o5
	movneg	%icc,	%o4,	%i4
	ldx	[%o7+4*32],	%o4
	movneg	%icc,	%o5,	%i5
	ldx	[%o7+12*32],%o5
	movcs	%xcc,	%o4,	%i4
	ldx	[%o7+5*32],%o4
	movcs	%xcc,	%o5,	%i5
	ldx	[%o7+13*32],%o5
	movvs	%xcc,	%o4,	%i4
	ldx	[%o7+6*32],	%o4
	movvs	%xcc,	%o5,	%i5
	ldx	[%o7+14*32],%o5
	move	%xcc,	%o4,	%i4
	ldx	[%o7+7*32],	%o4
	move	%xcc,	%o5,	%i5
	ldx	[%o7+15*32],%o5
	movneg	%xcc,	%o4,	%i4
	add	%o7,16*32,	%o7
	movneg	%xcc,	%o5,	%i5
	ldx	[%o7+0*32],	%l0
	ldx	[%o7+8*32],	%l1
	ldx	[%o7+1*32],	%o4
	ldx	[%o7+9*32],	%o5
	movvs	%icc,	%o4,	%l0
	ldx	[%o7+2*32],	%o4
	movvs	%icc,	%o5,	%l1
	ldx	[%o7+10*32],%o5
	move	%icc,	%o4,	%l0
	ldx	[%o7+3*32],	%o4
	move	%icc,	%o5,	%l1
	ldx	[%o7+11*32],%o5
	movneg	%icc,	%o4,	%l0
	ldx	[%o7+4*32],	%o4
	movneg	%icc,	%o5,	%l1
	ldx	[%o7+12*32],%o5
	movcs	%xcc,	%o4,	%l0
	ldx	[%o7+5*32],%o4
	movcs	%xcc,	%o5,	%l1
	ldx	[%o7+13*32],%o5
	movvs	%xcc,	%o4,	%l0
	ldx	[%o7+6*32],	%o4
	movvs	%xcc,	%o5,	%l1
	ldx	[%o7+14*32],%o5
	move	%xcc,	%o4,	%l0
	ldx	[%o7+7*32],	%o4
	move	%xcc,	%o5,	%l1
	ldx	[%o7+15*32],%o5
	movneg	%xcc,	%o4,	%l0
	add	%o7,16*32,	%o7
	movneg	%xcc,	%o5,	%l1
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	srax	%g4,	32,	%o4		! unpack %g4
	srl	%g4,	%g0,	%o5
	sub	%o4,	5,	%o4
	mov	%g3,	%i7
	sllx	%o4,	32,	%g4		! re-pack %g4
	or	%o5,	%g4,	%g4
	srl	%o5,	%o4,	%o5
	srl	%o5,	2,	%o4
	and	%o5,	3,	%o5
	and	%o4,	7,	%o4
	sll	%o5,	3,	%o5	! offset within first cache line
	add	%o5,	%i7,	%i7	! of the pwrtbl
	or	%g0,	1,	%o5
	sll	%o5,	%o4,	%o4
	.word	0x81b02940+8-1	! montsqr	8-1
	fbu,pn	%fcc3,.Labort_8
#ifndef	__arch64__
	and	%fp,%g5,%g5
	brz,pn	%g5,.Labort_8
#endif
	nop
	.word	0x81b02940+8-1	! montsqr	8-1
	fbu,pn	%fcc3,.Labort_8
#ifndef	__arch64__
	and	%fp,%g5,%g5
	brz,pn	%g5,.Labort_8
#endif
	nop
	.word	0x81b02940+8-1	! montsqr	8-1
	fbu,pn	%fcc3,.Labort_8
#ifndef	__arch64__
	and	%fp,%g5,%g5
	brz,pn	%g5,.Labort_8
#endif
	nop
	.word	0x81b02940+8-1	! montsqr	8-1
	fbu,pn	%fcc3,.Labort_8
#ifndef	__arch64__
	and	%fp,%g5,%g5
	brz,pn	%g5,.Labort_8
#endif
	nop
	.word	0x81b02940+8-1	! montsqr	8-1
	fbu,pn	%fcc3,.Labort_8
#ifndef	__arch64__
	and	%fp,%g5,%g5
	brz,pn	%g5,.Labort_8
#endif
	nop
	wr	%o4,	%g0,	%ccr
	.word	0x81b02920+8-1	! montmul	8-1
	fbu,pn	%fcc3,.Labort_8
#ifndef	__arch64__
	and	%fp,%g5,%g5
	brz,pn	%g5,.Labort_8
#endif

	srax	%g4,	32,	%o4
#ifdef	__arch64__
	brgez	%o4,.Lstride_8
	restore
	restore
	restore
	restore
	restore
#else
	brgez	%o4,.Lstride_8
	restore;		and	%fp,%g5,%g5
	restore;		and	%fp,%g5,%g5
	restore;		and	%fp,%g5,%g5
	restore;		and	%fp,%g5,%g5
	 brz,pn	%g5,.Labort1_8
	restore
#endif
	.word	0x81b02310 !movxtod	%l0,%f0
	.word	0x85b02311 !movxtod	%l1,%f2
	.word	0x89b02312 !movxtod	%l2,%f4
	.word	0x8db02313 !movxtod	%l3,%f6
	.word	0x91b02314 !movxtod	%l4,%f8
	.word	0x95b02315 !movxtod	%l5,%f10
	.word	0x99b02316 !movxtod	%l6,%f12
	.word	0x9db02317 !movxtod	%l7,%f14
#ifdef	__arch64__
	restore
#else
	 and	%fp,%g5,%g5
	restore
	 and	%g5,1,%o7
	 and	%fp,%g5,%g5
	 srl	%fp,0,%fp		! just in case?
	 or	%o7,%g5,%g5
	brz,a,pn %g5,.Ldone_8
	mov	0,%i0		! return failure
#endif
	std	%f0,[%g1+0*8]
	std	%f2,[%g1+1*8]
	std	%f4,[%g1+2*8]
	std	%f6,[%g1+3*8]
	std	%f8,[%g1+4*8]
	std	%f10,[%g1+5*8]
	std	%f12,[%g1+6*8]
	std	%f14,[%g1+7*8]
	mov	1,%i0		! return success
.Ldone_8:
	ret
	restore

.Labort_8:
	restore
	restore
	restore
	restore
	restore
.Labort1_8:
	restore

	mov	0,%i0		! return failure
	ret
	restore
.type	bn_pwr5_mont_t4_8, #function
.size	bn_pwr5_mont_t4_8, .-bn_pwr5_mont_t4_8
.globl	bn_pwr5_mont_t4_16
.align	32
bn_pwr5_mont_t4_16:
#ifdef	__arch64__
	mov	0,%g5
	mov	-128,%g4
#elif defined(SPARCV9_64BIT_STACK)
	SPARC_LOAD_ADDRESS_LEAF(OPENSSL_sparcv9cap_P,%g1,%g5)
	ld	[%g1+0],%g1	! OPENSSL_sparcv9_P[0]
	mov	-2047,%g4
	and	%g1,SPARCV9_64BIT_STACK,%g1
	movrz	%g1,0,%g4
	mov	-1,%g5
	add	%g4,-128,%g4
#else
	mov	-1,%g5
	mov	-128,%g4
#endif
	sllx	%g5,32,%g5
	save	%sp,%g4,%sp
#ifndef	__arch64__
	save	%sp,-128,%sp	! warm it up
	save	%sp,-128,%sp
	save	%sp,-128,%sp
	save	%sp,-128,%sp
	save	%sp,-128,%sp
	save	%sp,-128,%sp
	restore
	restore
	restore
	restore
	restore
	restore
#endif
	and	%sp,1,%g4
	or	%g5,%fp,%fp
	or	%g4,%g5,%g5

	! copy arguments to global registers
	mov	%i0,%g1
	mov	%i1,%g2
	ld	[%i2+0],%f1	! load *n0
	ld	[%i2+4],%f0
	mov	%i3,%g3
	srl	%i4,%g0,%i4	! pack last arguments
	sllx	%i5,32,%g4
	or	%i4,%g4,%g4
	.word	0xbbb00f00 !fsrc2	%f0,%f0,%f60
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	ldx	[%g1+0*8],%l0
	ldx	[%g1+1*8],%l1
	ldx	[%g1+2*8],%l2
	ldx	[%g1+3*8],%l3
	ldx	[%g1+4*8],%l4
	ldx	[%g1+5*8],%l5
	ldx	[%g1+6*8],%l6
	ldx	[%g1+7*8],%l7
	ldx	[%g1+8*8],%o0
	ldx	[%g1+9*8],%o1
	ldx	[%g1+10*8],%o2
	ldx	[%g1+11*8],%o3
	ldx	[%g1+12*8],%o4
	ldx	[%g1+13*8],%o5
	ldd	[%g1+14*8],%f24
	ldd	[%g1+15*8],%f26
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	ldx	[%g2+0*8],%l0
	ldx	[%g2+1*8],%l1
	ldx	[%g2+2*8],%l2
	ldx	[%g2+3*8],%l3
	ldx	[%g2+4*8],%l4
	ldx	[%g2+5*8],%l5
	ldx	[%g2+6*8],%l6
	ldx	[%g2+7*8],%l7
	ldx	[%g2+8*8],%o0
	ldx	[%g2+9*8],%o1
	ldx	[%g2+10*8],%o2
	ldx	[%g2+11*8],%o3
	ldx	[%g2+12*8],%o4
	ldx	[%g2+13*8],%o5
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	ldx	[%g2+14*8],%l0
	ldx	[%g2+15*8],%l1
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	save	%sp,-128,%sp;		or	%g5,%fp,%fp

	srlx	%g4,	32,	%o4		! unpack %g4
	srl	%g4,	%g0,	%o5
	sub	%o4,	5,	%o4
	mov	%g3,	%o7
	sllx	%o4,	32,	%g4		! re-pack %g4
	or	%o5,	%g4,	%g4
	srl	%o5,	%o4,	%o5
	srl	%o5,	2,	%o4
	and	%o5,	3,	%o5
	and	%o4,	7,	%o4
	sll	%o5,	3,	%o5	! offset within first cache line
	add	%o5,	%o7,	%o7	! of the pwrtbl
	or	%g0,	1,	%o5
	sll	%o5,	%o4,	%o4
	wr	%o4,	%g0,	%ccr
	b	.Lstride_16
	nop
.align	16
.Lstride_16:
	ldx	[%o7+0*32],	%i0
	ldx	[%o7+8*32],	%i1
	ldx	[%o7+1*32],	%o4
	ldx	[%o7+9*32],	%o5
	movvs	%icc,	%o4,	%i0
	ldx	[%o7+2*32],	%o4
	movvs	%icc,	%o5,	%i1
	ldx	[%o7+10*32],%o5
	move	%icc,	%o4,	%i0
	ldx	[%o7+3*32],	%o4
	move	%icc,	%o5,	%i1
	ldx	[%o7+11*32],%o5
	movneg	%icc,	%o4,	%i0
	ldx	[%o7+4*32],	%o4
	movneg	%icc,	%o5,	%i1
	ldx	[%o7+12*32],%o5
	movcs	%xcc,	%o4,	%i0
	ldx	[%o7+5*32],%o4
	movcs	%xcc,	%o5,	%i1
	ldx	[%o7+13*32],%o5
	movvs	%xcc,	%o4,	%i0
	ldx	[%o7+6*32],	%o4
	movvs	%xcc,	%o5,	%i1
	ldx	[%o7+14*32],%o5
	move	%xcc,	%o4,	%i0
	ldx	[%o7+7*32],	%o4
	move	%xcc,	%o5,	%i1
	ldx	[%o7+15*32],%o5
	movneg	%xcc,	%o4,	%i0
	add	%o7,16*32,	%o7
	movneg	%xcc,	%o5,	%i1
	ldx	[%o7+0*32],	%i2
	ldx	[%o7+8*32],	%i3
	ldx	[%o7+1*32],	%o4
	ldx	[%o7+9*32],	%o5
	movvs	%icc,	%o4,	%i2
	ldx	[%o7+2*32],	%o4
	movvs	%icc,	%o5,	%i3
	ldx	[%o7+10*32],%o5
	move	%icc,	%o4,	%i2
	ldx	[%o7+3*32],	%o4
	move	%icc,	%o5,	%i3
	ldx	[%o7+11*32],%o5
	movneg	%icc,	%o4,	%i2
	ldx	[%o7+4*32],	%o4
	movneg	%icc,	%o5,	%i3
	ldx	[%o7+12*32],%o5
	movcs	%xcc,	%o4,	%i2
	ldx	[%o7+5*32],%o4
	movcs	%xcc,	%o5,	%i3
	ldx	[%o7+13*32],%o5
	movvs	%xcc,	%o4,	%i2
	ldx	[%o7+6*32],	%o4
	movvs	%xcc,	%o5,	%i3
	ldx	[%o7+14*32],%o5
	move	%xcc,	%o4,	%i2
	ldx	[%o7+7*32],	%o4
	move	%xcc,	%o5,	%i3
	ldx	[%o7+15*32],%o5
	movneg	%xcc,	%o4,	%i2
	add	%o7,16*32,	%o7
	movneg	%xcc,	%o5,	%i3
	ldx	[%o7+0*32],	%i4
	ldx	[%o7+8*32],	%i5
	ldx	[%o7+1*32],	%o4
	ldx	[%o7+9*32],	%o5
	movvs	%icc,	%o4,	%i4
	ldx	[%o7+2*32],	%o4
	movvs	%icc,	%o5,	%i5
	ldx	[%o7+10*32],%o5
	move	%icc,	%o4,	%i4
	ldx	[%o7+3*32],	%o4
	move	%icc,	%o5,	%i5
	ldx	[%o7+11*32],%o5
	movneg	%icc,	%o4,	%i4
	ldx	[%o7+4*32],	%o4
	movneg	%icc,	%o5,	%i5
	ldx	[%o7+12*32],%o5
	movcs	%xcc,	%o4,	%i4
	ldx	[%o7+5*32],%o4
	movcs	%xcc,	%o5,	%i5
	ldx	[%o7+13*32],%o5
	movvs	%xcc,	%o4,	%i4
	ldx	[%o7+6*32],	%o4
	movvs	%xcc,	%o5,	%i5
	ldx	[%o7+14*32],%o5
	move	%xcc,	%o4,	%i4
	ldx	[%o7+7*32],	%o4
	move	%xcc,	%o5,	%i5
	ldx	[%o7+15*32],%o5
	movneg	%xcc,	%o4,	%i4
	add	%o7,16*32,	%o7
	movneg	%xcc,	%o5,	%i5
	ldx	[%o7+0*32],	%l0
	ldx	[%o7+8*32],	%l1
	ldx	[%o7+1*32],	%o4
	ldx	[%o7+9*32],	%o5
	movvs	%icc,	%o4,	%l0
	ldx	[%o7+2*32],	%o4
	movvs	%icc,	%o5,	%l1
	ldx	[%o7+10*32],%o5
	move	%icc,	%o4,	%l0
	ldx	[%o7+3*32],	%o4
	move	%icc,	%o5,	%l1
	ldx	[%o7+11*32],%o5
	movneg	%icc,	%o4,	%l0
	ldx	[%o7+4*32],	%o4
	movneg	%icc,	%o5,	%l1
	ldx	[%o7+12*32],%o5
	movcs	%xcc,	%o4,	%l0
	ldx	[%o7+5*32],%o4
	movcs	%xcc,	%o5,	%l1
	ldx	[%o7+13*32],%o5
	movvs	%xcc,	%o4,	%l0
	ldx	[%o7+6*32],	%o4
	movvs	%xcc,	%o5,	%l1
	ldx	[%o7+14*32],%o5
	move	%xcc,	%o4,	%l0
	ldx	[%o7+7*32],	%o4
	move	%xcc,	%o5,	%l1
	ldx	[%o7+15*32],%o5
	movneg	%xcc,	%o4,	%l0
	add	%o7,16*32,	%o7
	movneg	%xcc,	%o5,	%l1
	ldx	[%o7+0*32],	%l2
	ldx	[%o7+8*32],	%l3
	ldx	[%o7+1*32],	%o4
	ldx	[%o7+9*32],	%o5
	movvs	%icc,	%o4,	%l2
	ldx	[%o7+2*32],	%o4
	movvs	%icc,	%o5,	%l3
	ldx	[%o7+10*32],%o5
	move	%icc,	%o4,	%l2
	ldx	[%o7+3*32],	%o4
	move	%icc,	%o5,	%l3
	ldx	[%o7+11*32],%o5
	movneg	%icc,	%o4,	%l2
	ldx	[%o7+4*32],	%o4
	movneg	%icc,	%o5,	%l3
	ldx	[%o7+12*32],%o5
	movcs	%xcc,	%o4,	%l2
	ldx	[%o7+5*32],%o4
	movcs	%xcc,	%o5,	%l3
	ldx	[%o7+13*32],%o5
	movvs	%xcc,	%o4,	%l2
	ldx	[%o7+6*32],	%o4
	movvs	%xcc,	%o5,	%l3
	ldx	[%o7+14*32],%o5
	move	%xcc,	%o4,	%l2
	ldx	[%o7+7*32],	%o4
	move	%xcc,	%o5,	%l3
	ldx	[%o7+15*32],%o5
	movneg	%xcc,	%o4,	%l2
	add	%o7,16*32,	%o7
	movneg	%xcc,	%o5,	%l3
	ldx	[%o7+0*32],	%l4
	ldx	[%o7+8*32],	%l5
	ldx	[%o7+1*32],	%o4
	ldx	[%o7+9*32],	%o5
	movvs	%icc,	%o4,	%l4
	ldx	[%o7+2*32],	%o4
	movvs	%icc,	%o5,	%l5
	ldx	[%o7+10*32],%o5
	move	%icc,	%o4,	%l4
	ldx	[%o7+3*32],	%o4
	move	%icc,	%o5,	%l5
	ldx	[%o7+11*32],%o5
	movneg	%icc,	%o4,	%l4
	ldx	[%o7+4*32],	%o4
	movneg	%icc,	%o5,	%l5
	ldx	[%o7+12*32],%o5
	movcs	%xcc,	%o4,	%l4
	ldx	[%o7+5*32],%o4
	movcs	%xcc,	%o5,	%l5
	ldx	[%o7+13*32],%o5
	movvs	%xcc,	%o4,	%l4
	ldx	[%o7+6*32],	%o4
	movvs	%xcc,	%o5,	%l5
	ldx	[%o7+14*32],%o5
	move	%xcc,	%o4,	%l4
	ldx	[%o7+7*32],	%o4
	move	%xcc,	%o5,	%l5
	ldx	[%o7+15*32],%o5
	movneg	%xcc,	%o4,	%l4
	add	%o7,16*32,	%o7
	movneg	%xcc,	%o5,	%l5
	ldx	[%o7+0*32],	%l6
	ldx	[%o7+8*32],	%l7
	ldx	[%o7+1*32],	%o4
	ldx	[%o7+9*32],	%o5
	movvs	%icc,	%o4,	%l6
	ldx	[%o7+2*32],	%o4
	movvs	%icc,	%o5,	%l7
	ldx	[%o7+10*32],%o5
	move	%icc,	%o4,	%l6
	ldx	[%o7+3*32],	%o4
	move	%icc,	%o5,	%l7
	ldx	[%o7+11*32],%o5
	movneg	%icc,	%o4,	%l6
	ldx	[%o7+4*32],	%o4
	movneg	%icc,	%o5,	%l7
	ldx	[%o7+12*32],%o5
	movcs	%xcc,	%o4,	%l6
	ldx	[%o7+5*32],%o4
	movcs	%xcc,	%o5,	%l7
	ldx	[%o7+13*32],%o5
	movvs	%xcc,	%o4,	%l6
	ldx	[%o7+6*32],	%o4
	movvs	%xcc,	%o5,	%l7
	ldx	[%o7+14*32],%o5
	move	%xcc,	%o4,	%l6
	ldx	[%o7+7*32],	%o4
	move	%xcc,	%o5,	%l7
	ldx	[%o7+15*32],%o5
	movneg	%xcc,	%o4,	%l6
	add	%o7,16*32,	%o7
	movneg	%xcc,	%o5,	%l7
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	ldx	[%i7+0*32],	%i0
	ldx	[%i7+8*32],	%i1
	ldx	[%i7+1*32],	%o4
	ldx	[%i7+9*32],	%o5
	movvs	%icc,	%o4,	%i0
	ldx	[%i7+2*32],	%o4
	movvs	%icc,	%o5,	%i1
	ldx	[%i7+10*32],%o5
	move	%icc,	%o4,	%i0
	ldx	[%i7+3*32],	%o4
	move	%icc,	%o5,	%i1
	ldx	[%i7+11*32],%o5
	movneg	%icc,	%o4,	%i0
	ldx	[%i7+4*32],	%o4
	movneg	%icc,	%o5,	%i1
	ldx	[%i7+12*32],%o5
	movcs	%xcc,	%o4,	%i0
	ldx	[%i7+5*32],%o4
	movcs	%xcc,	%o5,	%i1
	ldx	[%i7+13*32],%o5
	movvs	%xcc,	%o4,	%i0
	ldx	[%i7+6*32],	%o4
	movvs	%xcc,	%o5,	%i1
	ldx	[%i7+14*32],%o5
	move	%xcc,	%o4,	%i0
	ldx	[%i7+7*32],	%o4
	move	%xcc,	%o5,	%i1
	ldx	[%i7+15*32],%o5
	movneg	%xcc,	%o4,	%i0
	add	%i7,16*32,	%i7
	movneg	%xcc,	%o5,	%i1
	srax	%g4,	32,	%o4		! unpack %g4
	srl	%g4,	%g0,	%o5
	sub	%o4,	5,	%o4
	mov	%g3,	%i7
	sllx	%o4,	32,	%g4		! re-pack %g4
	or	%o5,	%g4,	%g4
	srl	%o5,	%o4,	%o5
	srl	%o5,	2,	%o4
	and	%o5,	3,	%o5
	and	%o4,	7,	%o4
	sll	%o5,	3,	%o5	! offset within first cache line
	add	%o5,	%i7,	%i7	! of the pwrtbl
	or	%g0,	1,	%o5
	sll	%o5,	%o4,	%o4
	.word	0x81b02940+16-1	! montsqr	16-1
	fbu,pn	%fcc3,.Labort_16
#ifndef	__arch64__
	and	%fp,%g5,%g5
	brz,pn	%g5,.Labort_16
#endif
	nop
	.word	0x81b02940+16-1	! montsqr	16-1
	fbu,pn	%fcc3,.Labort_16
#ifndef	__arch64__
	and	%fp,%g5,%g5
	brz,pn	%g5,.Labort_16
#endif
	nop
	.word	0x81b02940+16-1	! montsqr	16-1
	fbu,pn	%fcc3,.Labort_16
#ifndef	__arch64__
	and	%fp,%g5,%g5
	brz,pn	%g5,.Labort_16
#endif
	nop
	.word	0x81b02940+16-1	! montsqr	16-1
	fbu,pn	%fcc3,.Labort_16
#ifndef	__arch64__
	and	%fp,%g5,%g5
	brz,pn	%g5,.Labort_16
#endif
	nop
	.word	0x81b02940+16-1	! montsqr	16-1
	fbu,pn	%fcc3,.Labort_16
#ifndef	__arch64__
	and	%fp,%g5,%g5
	brz,pn	%g5,.Labort_16
#endif
	nop
	wr	%o4,	%g0,	%ccr
	.word	0x81b02920+16-1	! montmul	16-1
	fbu,pn	%fcc3,.Labort_16
#ifndef	__arch64__
	and	%fp,%g5,%g5
	brz,pn	%g5,.Labort_16
#endif

	srax	%g4,	32,	%o4
#ifdef	__arch64__
	brgez	%o4,.Lstride_16
	restore
	restore
	restore
	restore
	restore
#else
	brgez	%o4,.Lstride_16
	restore;		and	%fp,%g5,%g5
	restore;		and	%fp,%g5,%g5
	restore;		and	%fp,%g5,%g5
	restore;		and	%fp,%g5,%g5
	 brz,pn	%g5,.Labort1_16
	restore
#endif
	.word	0x81b02310 !movxtod	%l0,%f0
	.word	0x85b02311 !movxtod	%l1,%f2
	.word	0x89b02312 !movxtod	%l2,%f4
	.word	0x8db02313 !movxtod	%l3,%f6
	.word	0x91b02314 !movxtod	%l4,%f8
	.word	0x95b02315 !movxtod	%l5,%f10
	.word	0x99b02316 !movxtod	%l6,%f12
	.word	0x9db02317 !movxtod	%l7,%f14
	.word	0xa1b02308 !movxtod	%o0,%f16
	.word	0xa5b02309 !movxtod	%o1,%f18
	.word	0xa9b0230a !movxtod	%o2,%f20
	.word	0xadb0230b !movxtod	%o3,%f22
	.word	0xbbb0230c !movxtod	%o4,%f60
	.word	0xbfb0230d !movxtod	%o5,%f62
#ifdef	__arch64__
	restore
#else
	 and	%fp,%g5,%g5
	restore
	 and	%g5,1,%o7
	 and	%fp,%g5,%g5
	 srl	%fp,0,%fp		! just in case?
	 or	%o7,%g5,%g5
	brz,a,pn %g5,.Ldone_16
	mov	0,%i0		! return failure
#endif
	std	%f0,[%g1+0*8]
	std	%f2,[%g1+1*8]
	std	%f4,[%g1+2*8]
	std	%f6,[%g1+3*8]
	std	%f8,[%g1+4*8]
	std	%f10,[%g1+5*8]
	std	%f12,[%g1+6*8]
	std	%f14,[%g1+7*8]
	std	%f16,[%g1+8*8]
	std	%f18,[%g1+9*8]
	std	%f20,[%g1+10*8]
	std	%f22,[%g1+11*8]
	std	%f60,[%g1+12*8]
	std	%f62,[%g1+13*8]
	std	%f24,[%g1+14*8]
	std	%f26,[%g1+15*8]
	mov	1,%i0		! return success
.Ldone_16:
	ret
	restore

.Labort_16:
	restore
	restore
	restore
	restore
	restore
.Labort1_16:
	restore

	mov	0,%i0		! return failure
	ret
	restore
.type	bn_pwr5_mont_t4_16, #function
.size	bn_pwr5_mont_t4_16, .-bn_pwr5_mont_t4_16
.globl	bn_pwr5_mont_t4_24
.align	32
bn_pwr5_mont_t4_24:
#ifdef	__arch64__
	mov	0,%g5
	mov	-128,%g4
#elif defined(SPARCV9_64BIT_STACK)
	SPARC_LOAD_ADDRESS_LEAF(OPENSSL_sparcv9cap_P,%g1,%g5)
	ld	[%g1+0],%g1	! OPENSSL_sparcv9_P[0]
	mov	-2047,%g4
	and	%g1,SPARCV9_64BIT_STACK,%g1
	movrz	%g1,0,%g4
	mov	-1,%g5
	add	%g4,-128,%g4
#else
	mov	-1,%g5
	mov	-128,%g4
#endif
	sllx	%g5,32,%g5
	save	%sp,%g4,%sp
#ifndef	__arch64__
	save	%sp,-128,%sp	! warm it up
	save	%sp,-128,%sp
	save	%sp,-128,%sp
	save	%sp,-128,%sp
	save	%sp,-128,%sp
	save	%sp,-128,%sp
	restore
	restore
	restore
	restore
	restore
	restore
#endif
	and	%sp,1,%g4
	or	%g5,%fp,%fp
	or	%g4,%g5,%g5

	! copy arguments to global registers
	mov	%i0,%g1
	mov	%i1,%g2
	ld	[%i2+0],%f1	! load *n0
	ld	[%i2+4],%f0
	mov	%i3,%g3
	srl	%i4,%g0,%i4	! pack last arguments
	sllx	%i5,32,%g4
	or	%i4,%g4,%g4
	.word	0xbbb00f00 !fsrc2	%f0,%f0,%f60
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	ldx	[%g1+0*8],%l0
	ldx	[%g1+1*8],%l1
	ldx	[%g1+2*8],%l2
	ldx	[%g1+3*8],%l3
	ldx	[%g1+4*8],%l4
	ldx	[%g1+5*8],%l5
	ldx	[%g1+6*8],%l6
	ldx	[%g1+7*8],%l7
	ldx	[%g1+8*8],%o0
	ldx	[%g1+9*8],%o1
	ldx	[%g1+10*8],%o2
	ldx	[%g1+11*8],%o3
	ldx	[%g1+12*8],%o4
	ldx	[%g1+13*8],%o5
	ldd	[%g1+14*8],%f24
	ldd	[%g1+15*8],%f26
	ldd	[%g1+16*8],%f28
	ldd	[%g1+17*8],%f30
	ldd	[%g1+18*8],%f32
	ldd	[%g1+19*8],%f34
	ldd	[%g1+20*8],%f36
	ldd	[%g1+21*8],%f38
	ldd	[%g1+22*8],%f40
	ldd	[%g1+23*8],%f42
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	ldx	[%g2+0*8],%l0
	ldx	[%g2+1*8],%l1
	ldx	[%g2+2*8],%l2
	ldx	[%g2+3*8],%l3
	ldx	[%g2+4*8],%l4
	ldx	[%g2+5*8],%l5
	ldx	[%g2+6*8],%l6
	ldx	[%g2+7*8],%l7
	ldx	[%g2+8*8],%o0
	ldx	[%g2+9*8],%o1
	ldx	[%g2+10*8],%o2
	ldx	[%g2+11*8],%o3
	ldx	[%g2+12*8],%o4
	ldx	[%g2+13*8],%o5
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	ldx	[%g2+14*8],%l0
	ldx	[%g2+15*8],%l1
	ldx	[%g2+16*8],%l2
	ldx	[%g2+17*8],%l3
	ldx	[%g2+18*8],%l4
	ldx	[%g2+19*8],%l5
	ldx	[%g2+20*8],%l6
	ldx	[%g2+21*8],%l7
	ldx	[%g2+22*8],%o0
	ldx	[%g2+23*8],%o1
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	save	%sp,-128,%sp;		or	%g5,%fp,%fp

	srlx	%g4,	32,	%o4		! unpack %g4
	srl	%g4,	%g0,	%o5
	sub	%o4,	5,	%o4
	mov	%g3,	%o7
	sllx	%o4,	32,	%g4		! re-pack %g4
	or	%o5,	%g4,	%g4
	srl	%o5,	%o4,	%o5
	srl	%o5,	2,	%o4
	and	%o5,	3,	%o5
	and	%o4,	7,	%o4
	sll	%o5,	3,	%o5	! offset within first cache line
	add	%o5,	%o7,	%o7	! of the pwrtbl
	or	%g0,	1,	%o5
	sll	%o5,	%o4,	%o4
	wr	%o4,	%g0,	%ccr
	b	.Lstride_24
	nop
.align	16
.Lstride_24:
	ldx	[%o7+0*32],	%i0
	ldx	[%o7+8*32],	%i1
	ldx	[%o7+1*32],	%o4
	ldx	[%o7+9*32],	%o5
	movvs	%icc,	%o4,	%i0
	ldx	[%o7+2*32],	%o4
	movvs	%icc,	%o5,	%i1
	ldx	[%o7+10*32],%o5
	move	%icc,	%o4,	%i0
	ldx	[%o7+3*32],	%o4
	move	%icc,	%o5,	%i1
	ldx	[%o7+11*32],%o5
	movneg	%icc,	%o4,	%i0
	ldx	[%o7+4*32],	%o4
	movneg	%icc,	%o5,	%i1
	ldx	[%o7+12*32],%o5
	movcs	%xcc,	%o4,	%i0
	ldx	[%o7+5*32],%o4
	movcs	%xcc,	%o5,	%i1
	ldx	[%o7+13*32],%o5
	movvs	%xcc,	%o4,	%i0
	ldx	[%o7+6*32],	%o4
	movvs	%xcc,	%o5,	%i1
	ldx	[%o7+14*32],%o5
	move	%xcc,	%o4,	%i0
	ldx	[%o7+7*32],	%o4
	move	%xcc,	%o5,	%i1
	ldx	[%o7+15*32],%o5
	movneg	%xcc,	%o4,	%i0
	add	%o7,16*32,	%o7
	movneg	%xcc,	%o5,	%i1
	ldx	[%o7+0*32],	%i2
	ldx	[%o7+8*32],	%i3
	ldx	[%o7+1*32],	%o4
	ldx	[%o7+9*32],	%o5
	movvs	%icc,	%o4,	%i2
	ldx	[%o7+2*32],	%o4
	movvs	%icc,	%o5,	%i3
	ldx	[%o7+10*32],%o5
	move	%icc,	%o4,	%i2
	ldx	[%o7+3*32],	%o4
	move	%icc,	%o5,	%i3
	ldx	[%o7+11*32],%o5
	movneg	%icc,	%o4,	%i2
	ldx	[%o7+4*32],	%o4
	movneg	%icc,	%o5,	%i3
	ldx	[%o7+12*32],%o5
	movcs	%xcc,	%o4,	%i2
	ldx	[%o7+5*32],%o4
	movcs	%xcc,	%o5,	%i3
	ldx	[%o7+13*32],%o5
	movvs	%xcc,	%o4,	%i2
	ldx	[%o7+6*32],	%o4
	movvs	%xcc,	%o5,	%i3
	ldx	[%o7+14*32],%o5
	move	%xcc,	%o4,	%i2
	ldx	[%o7+7*32],	%o4
	move	%xcc,	%o5,	%i3
	ldx	[%o7+15*32],%o5
	movneg	%xcc,	%o4,	%i2
	add	%o7,16*32,	%o7
	movneg	%xcc,	%o5,	%i3
	ldx	[%o7+0*32],	%i4
	ldx	[%o7+8*32],	%i5
	ldx	[%o7+1*32],	%o4
	ldx	[%o7+9*32],	%o5
	movvs	%icc,	%o4,	%i4
	ldx	[%o7+2*32],	%o4
	movvs	%icc,	%o5,	%i5
	ldx	[%o7+10*32],%o5
	move	%icc,	%o4,	%i4
	ldx	[%o7+3*32],	%o4
	move	%icc,	%o5,	%i5
	ldx	[%o7+11*32],%o5
	movneg	%icc,	%o4,	%i4
	ldx	[%o7+4*32],	%o4
	movneg	%icc,	%o5,	%i5
	ldx	[%o7+12*32],%o5
	movcs	%xcc,	%o4,	%i4
	ldx	[%o7+5*32],%o4
	movcs	%xcc,	%o5,	%i5
	ldx	[%o7+13*32],%o5
	movvs	%xcc,	%o4,	%i4
	ldx	[%o7+6*32],	%o4
	movvs	%xcc,	%o5,	%i5
	ldx	[%o7+14*32],%o5
	move	%xcc,	%o4,	%i4
	ldx	[%o7+7*32],	%o4
	move	%xcc,	%o5,	%i5
	ldx	[%o7+15*32],%o5
	movneg	%xcc,	%o4,	%i4
	add	%o7,16*32,	%o7
	movneg	%xcc,	%o5,	%i5
	ldx	[%o7+0*32],	%l0
	ldx	[%o7+8*32],	%l1
	ldx	[%o7+1*32],	%o4
	ldx	[%o7+9*32],	%o5
	movvs	%icc,	%o4,	%l0
	ldx	[%o7+2*32],	%o4
	movvs	%icc,	%o5,	%l1
	ldx	[%o7+10*32],%o5
	move	%icc,	%o4,	%l0
	ldx	[%o7+3*32],	%o4
	move	%icc,	%o5,	%l1
	ldx	[%o7+11*32],%o5
	movneg	%icc,	%o4,	%l0
	ldx	[%o7+4*32],	%o4
	movneg	%icc,	%o5,	%l1
	ldx	[%o7+12*32],%o5
	movcs	%xcc,	%o4,	%l0
	ldx	[%o7+5*32],%o4
	movcs	%xcc,	%o5,	%l1
	ldx	[%o7+13*32],%o5
	movvs	%xcc,	%o4,	%l0
	ldx	[%o7+6*32],	%o4
	movvs	%xcc,	%o5,	%l1
	ldx	[%o7+14*32],%o5
	move	%xcc,	%o4,	%l0
	ldx	[%o7+7*32],	%o4
	move	%xcc,	%o5,	%l1
	ldx	[%o7+15*32],%o5
	movneg	%xcc,	%o4,	%l0
	add	%o7,16*32,	%o7
	movneg	%xcc,	%o5,	%l1
	ldx	[%o7+0*32],	%l2
	ldx	[%o7+8*32],	%l3
	ldx	[%o7+1*32],	%o4
	ldx	[%o7+9*32],	%o5
	movvs	%icc,	%o4,	%l2
	ldx	[%o7+2*32],	%o4
	movvs	%icc,	%o5,	%l3
	ldx	[%o7+10*32],%o5
	move	%icc,	%o4,	%l2
	ldx	[%o7+3*32],	%o4
	move	%icc,	%o5,	%l3
	ldx	[%o7+11*32],%o5
	movneg	%icc,	%o4,	%l2
	ldx	[%o7+4*32],	%o4
	movneg	%icc,	%o5,	%l3
	ldx	[%o7+12*32],%o5
	movcs	%xcc,	%o4,	%l2
	ldx	[%o7+5*32],%o4
	movcs	%xcc,	%o5,	%l3
	ldx	[%o7+13*32],%o5
	movvs	%xcc,	%o4,	%l2
	ldx	[%o7+6*32],	%o4
	movvs	%xcc,	%o5,	%l3
	ldx	[%o7+14*32],%o5
	move	%xcc,	%o4,	%l2
	ldx	[%o7+7*32],	%o4
	move	%xcc,	%o5,	%l3
	ldx	[%o7+15*32],%o5
	movneg	%xcc,	%o4,	%l2
	add	%o7,16*32,	%o7
	movneg	%xcc,	%o5,	%l3
	ldx	[%o7+0*32],	%l4
	ldx	[%o7+8*32],	%l5
	ldx	[%o7+1*32],	%o4
	ldx	[%o7+9*32],	%o5
	movvs	%icc,	%o4,	%l4
	ldx	[%o7+2*32],	%o4
	movvs	%icc,	%o5,	%l5
	ldx	[%o7+10*32],%o5
	move	%icc,	%o4,	%l4
	ldx	[%o7+3*32],	%o4
	move	%icc,	%o5,	%l5
	ldx	[%o7+11*32],%o5
	movneg	%icc,	%o4,	%l4
	ldx	[%o7+4*32],	%o4
	movneg	%icc,	%o5,	%l5
	ldx	[%o7+12*32],%o5
	movcs	%xcc,	%o4,	%l4
	ldx	[%o7+5*32],%o4
	movcs	%xcc,	%o5,	%l5
	ldx	[%o7+13*32],%o5
	movvs	%xcc,	%o4,	%l4
	ldx	[%o7+6*32],	%o4
	movvs	%xcc,	%o5,	%l5
	ldx	[%o7+14*32],%o5
	move	%xcc,	%o4,	%l4
	ldx	[%o7+7*32],	%o4
	move	%xcc,	%o5,	%l5
	ldx	[%o7+15*32],%o5
	movneg	%xcc,	%o4,	%l4
	add	%o7,16*32,	%o7
	movneg	%xcc,	%o5,	%l5
	ldx	[%o7+0*32],	%l6
	ldx	[%o7+8*32],	%l7
	ldx	[%o7+1*32],	%o4
	ldx	[%o7+9*32],	%o5
	movvs	%icc,	%o4,	%l6
	ldx	[%o7+2*32],	%o4
	movvs	%icc,	%o5,	%l7
	ldx	[%o7+10*32],%o5
	move	%icc,	%o4,	%l6
	ldx	[%o7+3*32],	%o4
	move	%icc,	%o5,	%l7
	ldx	[%o7+11*32],%o5
	movneg	%icc,	%o4,	%l6
	ldx	[%o7+4*32],	%o4
	movneg	%icc,	%o5,	%l7
	ldx	[%o7+12*32],%o5
	movcs	%xcc,	%o4,	%l6
	ldx	[%o7+5*32],%o4
	movcs	%xcc,	%o5,	%l7
	ldx	[%o7+13*32],%o5
	movvs	%xcc,	%o4,	%l6
	ldx	[%o7+6*32],	%o4
	movvs	%xcc,	%o5,	%l7
	ldx	[%o7+14*32],%o5
	move	%xcc,	%o4,	%l6
	ldx	[%o7+7*32],	%o4
	move	%xcc,	%o5,	%l7
	ldx	[%o7+15*32],%o5
	movneg	%xcc,	%o4,	%l6
	add	%o7,16*32,	%o7
	movneg	%xcc,	%o5,	%l7
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	ldx	[%i7+0*32],	%i0
	ldx	[%i7+8*32],	%i1
	ldx	[%i7+1*32],	%o4
	ldx	[%i7+9*32],	%o5
	movvs	%icc,	%o4,	%i0
	ldx	[%i7+2*32],	%o4
	movvs	%icc,	%o5,	%i1
	ldx	[%i7+10*32],%o5
	move	%icc,	%o4,	%i0
	ldx	[%i7+3*32],	%o4
	move	%icc,	%o5,	%i1
	ldx	[%i7+11*32],%o5
	movneg	%icc,	%o4,	%i0
	ldx	[%i7+4*32],	%o4
	movneg	%icc,	%o5,	%i1
	ldx	[%i7+12*32],%o5
	movcs	%xcc,	%o4,	%i0
	ldx	[%i7+5*32],%o4
	movcs	%xcc,	%o5,	%i1
	ldx	[%i7+13*32],%o5
	movvs	%xcc,	%o4,	%i0
	ldx	[%i7+6*32],	%o4
	movvs	%xcc,	%o5,	%i1
	ldx	[%i7+14*32],%o5
	move	%xcc,	%o4,	%i0
	ldx	[%i7+7*32],	%o4
	move	%xcc,	%o5,	%i1
	ldx	[%i7+15*32],%o5
	movneg	%xcc,	%o4,	%i0
	add	%i7,16*32,	%i7
	movneg	%xcc,	%o5,	%i1
	ldx	[%i7+0*32],	%i2
	ldx	[%i7+8*32],	%i3
	ldx	[%i7+1*32],	%o4
	ldx	[%i7+9*32],	%o5
	movvs	%icc,	%o4,	%i2
	ldx	[%i7+2*32],	%o4
	movvs	%icc,	%o5,	%i3
	ldx	[%i7+10*32],%o5
	move	%icc,	%o4,	%i2
	ldx	[%i7+3*32],	%o4
	move	%icc,	%o5,	%i3
	ldx	[%i7+11*32],%o5
	movneg	%icc,	%o4,	%i2
	ldx	[%i7+4*32],	%o4
	movneg	%icc,	%o5,	%i3
	ldx	[%i7+12*32],%o5
	movcs	%xcc,	%o4,	%i2
	ldx	[%i7+5*32],%o4
	movcs	%xcc,	%o5,	%i3
	ldx	[%i7+13*32],%o5
	movvs	%xcc,	%o4,	%i2
	ldx	[%i7+6*32],	%o4
	movvs	%xcc,	%o5,	%i3
	ldx	[%i7+14*32],%o5
	move	%xcc,	%o4,	%i2
	ldx	[%i7+7*32],	%o4
	move	%xcc,	%o5,	%i3
	ldx	[%i7+15*32],%o5
	movneg	%xcc,	%o4,	%i2
	add	%i7,16*32,	%i7
	movneg	%xcc,	%o5,	%i3
	ldx	[%i7+0*32],	%i4
	ldx	[%i7+8*32],	%i5
	ldx	[%i7+1*32],	%o4
	ldx	[%i7+9*32],	%o5
	movvs	%icc,	%o4,	%i4
	ldx	[%i7+2*32],	%o4
	movvs	%icc,	%o5,	%i5
	ldx	[%i7+10*32],%o5
	move	%icc,	%o4,	%i4
	ldx	[%i7+3*32],	%o4
	move	%icc,	%o5,	%i5
	ldx	[%i7+11*32],%o5
	movneg	%icc,	%o4,	%i4
	ldx	[%i7+4*32],	%o4
	movneg	%icc,	%o5,	%i5
	ldx	[%i7+12*32],%o5
	movcs	%xcc,	%o4,	%i4
	ldx	[%i7+5*32],%o4
	movcs	%xcc,	%o5,	%i5
	ldx	[%i7+13*32],%o5
	movvs	%xcc,	%o4,	%i4
	ldx	[%i7+6*32],	%o4
	movvs	%xcc,	%o5,	%i5
	ldx	[%i7+14*32],%o5
	move	%xcc,	%o4,	%i4
	ldx	[%i7+7*32],	%o4
	move	%xcc,	%o5,	%i5
	ldx	[%i7+15*32],%o5
	movneg	%xcc,	%o4,	%i4
	add	%i7,16*32,	%i7
	movneg	%xcc,	%o5,	%i5
	ldx	[%i7+0*32],	%l0
	ldx	[%i7+8*32],	%l1
	ldx	[%i7+1*32],	%o4
	ldx	[%i7+9*32],	%o5
	movvs	%icc,	%o4,	%l0
	ldx	[%i7+2*32],	%o4
	movvs	%icc,	%o5,	%l1
	ldx	[%i7+10*32],%o5
	move	%icc,	%o4,	%l0
	ldx	[%i7+3*32],	%o4
	move	%icc,	%o5,	%l1
	ldx	[%i7+11*32],%o5
	movneg	%icc,	%o4,	%l0
	ldx	[%i7+4*32],	%o4
	movneg	%icc,	%o5,	%l1
	ldx	[%i7+12*32],%o5
	movcs	%xcc,	%o4,	%l0
	ldx	[%i7+5*32],%o4
	movcs	%xcc,	%o5,	%l1
	ldx	[%i7+13*32],%o5
	movvs	%xcc,	%o4,	%l0
	ldx	[%i7+6*32],	%o4
	movvs	%xcc,	%o5,	%l1
	ldx	[%i7+14*32],%o5
	move	%xcc,	%o4,	%l0
	ldx	[%i7+7*32],	%o4
	move	%xcc,	%o5,	%l1
	ldx	[%i7+15*32],%o5
	movneg	%xcc,	%o4,	%l0
	add	%i7,16*32,	%i7
	movneg	%xcc,	%o5,	%l1
	ldx	[%i7+0*32],	%l2
	ldx	[%i7+8*32],	%l3
	ldx	[%i7+1*32],	%o4
	ldx	[%i7+9*32],	%o5
	movvs	%icc,	%o4,	%l2
	ldx	[%i7+2*32],	%o4
	movvs	%icc,	%o5,	%l3
	ldx	[%i7+10*32],%o5
	move	%icc,	%o4,	%l2
	ldx	[%i7+3*32],	%o4
	move	%icc,	%o5,	%l3
	ldx	[%i7+11*32],%o5
	movneg	%icc,	%o4,	%l2
	ldx	[%i7+4*32],	%o4
	movneg	%icc,	%o5,	%l3
	ldx	[%i7+12*32],%o5
	movcs	%xcc,	%o4,	%l2
	ldx	[%i7+5*32],%o4
	movcs	%xcc,	%o5,	%l3
	ldx	[%i7+13*32],%o5
	movvs	%xcc,	%o4,	%l2
	ldx	[%i7+6*32],	%o4
	movvs	%xcc,	%o5,	%l3
	ldx	[%i7+14*32],%o5
	move	%xcc,	%o4,	%l2
	ldx	[%i7+7*32],	%o4
	move	%xcc,	%o5,	%l3
	ldx	[%i7+15*32],%o5
	movneg	%xcc,	%o4,	%l2
	add	%i7,16*32,	%i7
	movneg	%xcc,	%o5,	%l3
	srax	%g4,	32,	%o4		! unpack %g4
	srl	%g4,	%g0,	%o5
	sub	%o4,	5,	%o4
	mov	%g3,	%i7
	sllx	%o4,	32,	%g4		! re-pack %g4
	or	%o5,	%g4,	%g4
	srl	%o5,	%o4,	%o5
	srl	%o5,	2,	%o4
	and	%o5,	3,	%o5
	and	%o4,	7,	%o4
	sll	%o5,	3,	%o5	! offset within first cache line
	add	%o5,	%i7,	%i7	! of the pwrtbl
	or	%g0,	1,	%o5
	sll	%o5,	%o4,	%o4
	.word	0x81b02940+24-1	! montsqr	24-1
	fbu,pn	%fcc3,.Labort_24
#ifndef	__arch64__
	and	%fp,%g5,%g5
	brz,pn	%g5,.Labort_24
#endif
	nop
	.word	0x81b02940+24-1	! montsqr	24-1
	fbu,pn	%fcc3,.Labort_24
#ifndef	__arch64__
	and	%fp,%g5,%g5
	brz,pn	%g5,.Labort_24
#endif
	nop
	.word	0x81b02940+24-1	! montsqr	24-1
	fbu,pn	%fcc3,.Labort_24
#ifndef	__arch64__
	and	%fp,%g5,%g5
	brz,pn	%g5,.Labort_24
#endif
	nop
	.word	0x81b02940+24-1	! montsqr	24-1
	fbu,pn	%fcc3,.Labort_24
#ifndef	__arch64__
	and	%fp,%g5,%g5
	brz,pn	%g5,.Labort_24
#endif
	nop
	.word	0x81b02940+24-1	! montsqr	24-1
	fbu,pn	%fcc3,.Labort_24
#ifndef	__arch64__
	and	%fp,%g5,%g5
	brz,pn	%g5,.Labort_24
#endif
	nop
	wr	%o4,	%g0,	%ccr
	.word	0x81b02920+24-1	! montmul	24-1
	fbu,pn	%fcc3,.Labort_24
#ifndef	__arch64__
	and	%fp,%g5,%g5
	brz,pn	%g5,.Labort_24
#endif

	srax	%g4,	32,	%o4
#ifdef	__arch64__
	brgez	%o4,.Lstride_24
	restore
	restore
	restore
	restore
	restore
#else
	brgez	%o4,.Lstride_24
	restore;		and	%fp,%g5,%g5
	restore;		and	%fp,%g5,%g5
	restore;		and	%fp,%g5,%g5
	restore;		and	%fp,%g5,%g5
	 brz,pn	%g5,.Labort1_24
	restore
#endif
	.word	0x81b02310 !movxtod	%l0,%f0
	.word	0x85b02311 !movxtod	%l1,%f2
	.word	0x89b02312 !movxtod	%l2,%f4
	.word	0x8db02313 !movxtod	%l3,%f6
	.word	0x91b02314 !movxtod	%l4,%f8
	.word	0x95b02315 !movxtod	%l5,%f10
	.word	0x99b02316 !movxtod	%l6,%f12
	.word	0x9db02317 !movxtod	%l7,%f14
	.word	0xa1b02308 !movxtod	%o0,%f16
	.word	0xa5b02309 !movxtod	%o1,%f18
	.word	0xa9b0230a !movxtod	%o2,%f20
	.word	0xadb0230b !movxtod	%o3,%f22
	.word	0xbbb0230c !movxtod	%o4,%f60
	.word	0xbfb0230d !movxtod	%o5,%f62
#ifdef	__arch64__
	restore
#else
	 and	%fp,%g5,%g5
	restore
	 and	%g5,1,%o7
	 and	%fp,%g5,%g5
	 srl	%fp,0,%fp		! just in case?
	 or	%o7,%g5,%g5
	brz,a,pn %g5,.Ldone_24
	mov	0,%i0		! return failure
#endif
	std	%f0,[%g1+0*8]
	std	%f2,[%g1+1*8]
	std	%f4,[%g1+2*8]
	std	%f6,[%g1+3*8]
	std	%f8,[%g1+4*8]
	std	%f10,[%g1+5*8]
	std	%f12,[%g1+6*8]
	std	%f14,[%g1+7*8]
	std	%f16,[%g1+8*8]
	std	%f18,[%g1+9*8]
	std	%f20,[%g1+10*8]
	std	%f22,[%g1+11*8]
	std	%f60,[%g1+12*8]
	std	%f62,[%g1+13*8]
	std	%f24,[%g1+14*8]
	std	%f26,[%g1+15*8]
	std	%f28,[%g1+16*8]
	std	%f30,[%g1+17*8]
	std	%f32,[%g1+18*8]
	std	%f34,[%g1+19*8]
	std	%f36,[%g1+20*8]
	std	%f38,[%g1+21*8]
	std	%f40,[%g1+22*8]
	std	%f42,[%g1+23*8]
	mov	1,%i0		! return success
.Ldone_24:
	ret
	restore

.Labort_24:
	restore
	restore
	restore
	restore
	restore
.Labort1_24:
	restore

	mov	0,%i0		! return failure
	ret
	restore
.type	bn_pwr5_mont_t4_24, #function
.size	bn_pwr5_mont_t4_24, .-bn_pwr5_mont_t4_24
.globl	bn_pwr5_mont_t4_32
.align	32
bn_pwr5_mont_t4_32:
#ifdef	__arch64__
	mov	0,%g5
	mov	-128,%g4
#elif defined(SPARCV9_64BIT_STACK)
	SPARC_LOAD_ADDRESS_LEAF(OPENSSL_sparcv9cap_P,%g1,%g5)
	ld	[%g1+0],%g1	! OPENSSL_sparcv9_P[0]
	mov	-2047,%g4
	and	%g1,SPARCV9_64BIT_STACK,%g1
	movrz	%g1,0,%g4
	mov	-1,%g5
	add	%g4,-128,%g4
#else
	mov	-1,%g5
	mov	-128,%g4
#endif
	sllx	%g5,32,%g5
	save	%sp,%g4,%sp
#ifndef	__arch64__
	save	%sp,-128,%sp	! warm it up
	save	%sp,-128,%sp
	save	%sp,-128,%sp
	save	%sp,-128,%sp
	save	%sp,-128,%sp
	save	%sp,-128,%sp
	restore
	restore
	restore
	restore
	restore
	restore
#endif
	and	%sp,1,%g4
	or	%g5,%fp,%fp
	or	%g4,%g5,%g5

	! copy arguments to global registers
	mov	%i0,%g1
	mov	%i1,%g2
	ld	[%i2+0],%f1	! load *n0
	ld	[%i2+4],%f0
	mov	%i3,%g3
	srl	%i4,%g0,%i4	! pack last arguments
	sllx	%i5,32,%g4
	or	%i4,%g4,%g4
	.word	0xbbb00f00 !fsrc2	%f0,%f0,%f60
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	ldx	[%g1+0*8],%l0
	ldx	[%g1+1*8],%l1
	ldx	[%g1+2*8],%l2
	ldx	[%g1+3*8],%l3
	ldx	[%g1+4*8],%l4
	ldx	[%g1+5*8],%l5
	ldx	[%g1+6*8],%l6
	ldx	[%g1+7*8],%l7
	ldx	[%g1+8*8],%o0
	ldx	[%g1+9*8],%o1
	ldx	[%g1+10*8],%o2
	ldx	[%g1+11*8],%o3
	ldx	[%g1+12*8],%o4
	ldx	[%g1+13*8],%o5
	ldd	[%g1+14*8],%f24
	ldd	[%g1+15*8],%f26
	ldd	[%g1+16*8],%f28
	ldd	[%g1+17*8],%f30
	ldd	[%g1+18*8],%f32
	ldd	[%g1+19*8],%f34
	ldd	[%g1+20*8],%f36
	ldd	[%g1+21*8],%f38
	ldd	[%g1+22*8],%f40
	ldd	[%g1+23*8],%f42
	ldd	[%g1+24*8],%f44
	ldd	[%g1+25*8],%f46
	ldd	[%g1+26*8],%f48
	ldd	[%g1+27*8],%f50
	ldd	[%g1+28*8],%f52
	ldd	[%g1+29*8],%f54
	ldd	[%g1+30*8],%f56
	ldd	[%g1+31*8],%f58
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	ldx	[%g2+0*8],%l0
	ldx	[%g2+1*8],%l1
	ldx	[%g2+2*8],%l2
	ldx	[%g2+3*8],%l3
	ldx	[%g2+4*8],%l4
	ldx	[%g2+5*8],%l5
	ldx	[%g2+6*8],%l6
	ldx	[%g2+7*8],%l7
	ldx	[%g2+8*8],%o0
	ldx	[%g2+9*8],%o1
	ldx	[%g2+10*8],%o2
	ldx	[%g2+11*8],%o3
	ldx	[%g2+12*8],%o4
	ldx	[%g2+13*8],%o5
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	ldx	[%g2+14*8],%l0
	ldx	[%g2+15*8],%l1
	ldx	[%g2+16*8],%l2
	ldx	[%g2+17*8],%l3
	ldx	[%g2+18*8],%l4
	ldx	[%g2+19*8],%l5
	ldx	[%g2+20*8],%l6
	ldx	[%g2+21*8],%l7
	ldx	[%g2+22*8],%o0
	ldx	[%g2+23*8],%o1
	ldx	[%g2+24*8],%o2
	ldx	[%g2+25*8],%o3
	ldx	[%g2+26*8],%o4
	ldx	[%g2+27*8],%o5
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	ldx	[%g2+28*8],%l0
	ldx	[%g2+29*8],%l1
	ldx	[%g2+30*8],%l2
	ldx	[%g2+31*8],%l3
	save	%sp,-128,%sp;		or	%g5,%fp,%fp

	srlx	%g4,	32,	%o4		! unpack %g4
	srl	%g4,	%g0,	%o5
	sub	%o4,	5,	%o4
	mov	%g3,	%o7
	sllx	%o4,	32,	%g4		! re-pack %g4
	or	%o5,	%g4,	%g4
	srl	%o5,	%o4,	%o5
	srl	%o5,	2,	%o4
	and	%o5,	3,	%o5
	and	%o4,	7,	%o4
	sll	%o5,	3,	%o5	! offset within first cache line
	add	%o5,	%o7,	%o7	! of the pwrtbl
	or	%g0,	1,	%o5
	sll	%o5,	%o4,	%o4
	wr	%o4,	%g0,	%ccr
	b	.Lstride_32
	nop
.align	16
.Lstride_32:
	ldx	[%o7+0*32],	%i0
	ldx	[%o7+8*32],	%i1
	ldx	[%o7+1*32],	%o4
	ldx	[%o7+9*32],	%o5
	movvs	%icc,	%o4,	%i0
	ldx	[%o7+2*32],	%o4
	movvs	%icc,	%o5,	%i1
	ldx	[%o7+10*32],%o5
	move	%icc,	%o4,	%i0
	ldx	[%o7+3*32],	%o4
	move	%icc,	%o5,	%i1
	ldx	[%o7+11*32],%o5
	movneg	%icc,	%o4,	%i0
	ldx	[%o7+4*32],	%o4
	movneg	%icc,	%o5,	%i1
	ldx	[%o7+12*32],%o5
	movcs	%xcc,	%o4,	%i0
	ldx	[%o7+5*32],%o4
	movcs	%xcc,	%o5,	%i1
	ldx	[%o7+13*32],%o5
	movvs	%xcc,	%o4,	%i0
	ldx	[%o7+6*32],	%o4
	movvs	%xcc,	%o5,	%i1
	ldx	[%o7+14*32],%o5
	move	%xcc,	%o4,	%i0
	ldx	[%o7+7*32],	%o4
	move	%xcc,	%o5,	%i1
	ldx	[%o7+15*32],%o5
	movneg	%xcc,	%o4,	%i0
	add	%o7,16*32,	%o7
	movneg	%xcc,	%o5,	%i1
	ldx	[%o7+0*32],	%i2
	ldx	[%o7+8*32],	%i3
	ldx	[%o7+1*32],	%o4
	ldx	[%o7+9*32],	%o5
	movvs	%icc,	%o4,	%i2
	ldx	[%o7+2*32],	%o4
	movvs	%icc,	%o5,	%i3
	ldx	[%o7+10*32],%o5
	move	%icc,	%o4,	%i2
	ldx	[%o7+3*32],	%o4
	move	%icc,	%o5,	%i3
	ldx	[%o7+11*32],%o5
	movneg	%icc,	%o4,	%i2
	ldx	[%o7+4*32],	%o4
	movneg	%icc,	%o5,	%i3
	ldx	[%o7+12*32],%o5
	movcs	%xcc,	%o4,	%i2
	ldx	[%o7+5*32],%o4
	movcs	%xcc,	%o5,	%i3
	ldx	[%o7+13*32],%o5
	movvs	%xcc,	%o4,	%i2
	ldx	[%o7+6*32],	%o4
	movvs	%xcc,	%o5,	%i3
	ldx	[%o7+14*32],%o5
	move	%xcc,	%o4,	%i2
	ldx	[%o7+7*32],	%o4
	move	%xcc,	%o5,	%i3
	ldx	[%o7+15*32],%o5
	movneg	%xcc,	%o4,	%i2
	add	%o7,16*32,	%o7
	movneg	%xcc,	%o5,	%i3
	ldx	[%o7+0*32],	%i4
	ldx	[%o7+8*32],	%i5
	ldx	[%o7+1*32],	%o4
	ldx	[%o7+9*32],	%o5
	movvs	%icc,	%o4,	%i4
	ldx	[%o7+2*32],	%o4
	movvs	%icc,	%o5,	%i5
	ldx	[%o7+10*32],%o5
	move	%icc,	%o4,	%i4
	ldx	[%o7+3*32],	%o4
	move	%icc,	%o5,	%i5
	ldx	[%o7+11*32],%o5
	movneg	%icc,	%o4,	%i4
	ldx	[%o7+4*32],	%o4
	movneg	%icc,	%o5,	%i5
	ldx	[%o7+12*32],%o5
	movcs	%xcc,	%o4,	%i4
	ldx	[%o7+5*32],%o4
	movcs	%xcc,	%o5,	%i5
	ldx	[%o7+13*32],%o5
	movvs	%xcc,	%o4,	%i4
	ldx	[%o7+6*32],	%o4
	movvs	%xcc,	%o5,	%i5
	ldx	[%o7+14*32],%o5
	move	%xcc,	%o4,	%i4
	ldx	[%o7+7*32],	%o4
	move	%xcc,	%o5,	%i5
	ldx	[%o7+15*32],%o5
	movneg	%xcc,	%o4,	%i4
	add	%o7,16*32,	%o7
	movneg	%xcc,	%o5,	%i5
	ldx	[%o7+0*32],	%l0
	ldx	[%o7+8*32],	%l1
	ldx	[%o7+1*32],	%o4
	ldx	[%o7+9*32],	%o5
	movvs	%icc,	%o4,	%l0
	ldx	[%o7+2*32],	%o4
	movvs	%icc,	%o5,	%l1
	ldx	[%o7+10*32],%o5
	move	%icc,	%o4,	%l0
	ldx	[%o7+3*32],	%o4
	move	%icc,	%o5,	%l1
	ldx	[%o7+11*32],%o5
	movneg	%icc,	%o4,	%l0
	ldx	[%o7+4*32],	%o4
	movneg	%icc,	%o5,	%l1
	ldx	[%o7+12*32],%o5
	movcs	%xcc,	%o4,	%l0
	ldx	[%o7+5*32],%o4
	movcs	%xcc,	%o5,	%l1
	ldx	[%o7+13*32],%o5
	movvs	%xcc,	%o4,	%l0
	ldx	[%o7+6*32],	%o4
	movvs	%xcc,	%o5,	%l1
	ldx	[%o7+14*32],%o5
	move	%xcc,	%o4,	%l0
	ldx	[%o7+7*32],	%o4
	move	%xcc,	%o5,	%l1
	ldx	[%o7+15*32],%o5
	movneg	%xcc,	%o4,	%l0
	add	%o7,16*32,	%o7
	movneg	%xcc,	%o5,	%l1
	ldx	[%o7+0*32],	%l2
	ldx	[%o7+8*32],	%l3
	ldx	[%o7+1*32],	%o4
	ldx	[%o7+9*32],	%o5
	movvs	%icc,	%o4,	%l2
	ldx	[%o7+2*32],	%o4
	movvs	%icc,	%o5,	%l3
	ldx	[%o7+10*32],%o5
	move	%icc,	%o4,	%l2
	ldx	[%o7+3*32],	%o4
	move	%icc,	%o5,	%l3
	ldx	[%o7+11*32],%o5
	movneg	%icc,	%o4,	%l2
	ldx	[%o7+4*32],	%o4
	movneg	%icc,	%o5,	%l3
	ldx	[%o7+12*32],%o5
	movcs	%xcc,	%o4,	%l2
	ldx	[%o7+5*32],%o4
	movcs	%xcc,	%o5,	%l3
	ldx	[%o7+13*32],%o5
	movvs	%xcc,	%o4,	%l2
	ldx	[%o7+6*32],	%o4
	movvs	%xcc,	%o5,	%l3
	ldx	[%o7+14*32],%o5
	move	%xcc,	%o4,	%l2
	ldx	[%o7+7*32],	%o4
	move	%xcc,	%o5,	%l3
	ldx	[%o7+15*32],%o5
	movneg	%xcc,	%o4,	%l2
	add	%o7,16*32,	%o7
	movneg	%xcc,	%o5,	%l3
	ldx	[%o7+0*32],	%l4
	ldx	[%o7+8*32],	%l5
	ldx	[%o7+1*32],	%o4
	ldx	[%o7+9*32],	%o5
	movvs	%icc,	%o4,	%l4
	ldx	[%o7+2*32],	%o4
	movvs	%icc,	%o5,	%l5
	ldx	[%o7+10*32],%o5
	move	%icc,	%o4,	%l4
	ldx	[%o7+3*32],	%o4
	move	%icc,	%o5,	%l5
	ldx	[%o7+11*32],%o5
	movneg	%icc,	%o4,	%l4
	ldx	[%o7+4*32],	%o4
	movneg	%icc,	%o5,	%l5
	ldx	[%o7+12*32],%o5
	movcs	%xcc,	%o4,	%l4
	ldx	[%o7+5*32],%o4
	movcs	%xcc,	%o5,	%l5
	ldx	[%o7+13*32],%o5
	movvs	%xcc,	%o4,	%l4
	ldx	[%o7+6*32],	%o4
	movvs	%xcc,	%o5,	%l5
	ldx	[%o7+14*32],%o5
	move	%xcc,	%o4,	%l4
	ldx	[%o7+7*32],	%o4
	move	%xcc,	%o5,	%l5
	ldx	[%o7+15*32],%o5
	movneg	%xcc,	%o4,	%l4
	add	%o7,16*32,	%o7
	movneg	%xcc,	%o5,	%l5
	ldx	[%o7+0*32],	%l6
	ldx	[%o7+8*32],	%l7
	ldx	[%o7+1*32],	%o4
	ldx	[%o7+9*32],	%o5
	movvs	%icc,	%o4,	%l6
	ldx	[%o7+2*32],	%o4
	movvs	%icc,	%o5,	%l7
	ldx	[%o7+10*32],%o5
	move	%icc,	%o4,	%l6
	ldx	[%o7+3*32],	%o4
	move	%icc,	%o5,	%l7
	ldx	[%o7+11*32],%o5
	movneg	%icc,	%o4,	%l6
	ldx	[%o7+4*32],	%o4
	movneg	%icc,	%o5,	%l7
	ldx	[%o7+12*32],%o5
	movcs	%xcc,	%o4,	%l6
	ldx	[%o7+5*32],%o4
	movcs	%xcc,	%o5,	%l7
	ldx	[%o7+13*32],%o5
	movvs	%xcc,	%o4,	%l6
	ldx	[%o7+6*32],	%o4
	movvs	%xcc,	%o5,	%l7
	ldx	[%o7+14*32],%o5
	move	%xcc,	%o4,	%l6
	ldx	[%o7+7*32],	%o4
	move	%xcc,	%o5,	%l7
	ldx	[%o7+15*32],%o5
	movneg	%xcc,	%o4,	%l6
	add	%o7,16*32,	%o7
	movneg	%xcc,	%o5,	%l7
	save	%sp,-128,%sp;		or	%g5,%fp,%fp
	ldx	[%i7+0*32],	%i0
	ldx	[%i7+8*32],	%i1
	ldx	[%i7+1*32],	%o4
	ldx	[%i7+9*32],	%o5
	movvs	%icc,	%o4,	%i0
	ldx	[%i7+2*32],	%o4
	movvs	%icc,	%o5,	%i1
	ldx	[%i7+10*32],%o5
	move	%icc,	%o4,	%i0
	ldx	[%i7+3*32],	%o4
	move	%icc,	%o5,	%i1
	ldx	[%i7+11*32],%o5
	movneg	%icc,	%o4,	%i0
	ldx	[%i7+4*32],	%o4
	movneg	%icc,	%o5,	%i1
	ldx	[%i7+12*32],%o5
	movcs	%xcc,	%o4,	%i0
	ldx	[%i7+5*32],%o4
	movcs	%xcc,	%o5,	%i1
	ldx	[%i7+13*32],%o5
	movvs	%xcc,	%o4,	%i0
	ldx	[%i7+6*32],	%o4
	movvs	%xcc,	%o5,	%i1
	ldx	[%i7+14*32],%o5
	move	%xcc,	%o4,	%i0
	ldx	[%i7+7*32],	%o4
	move	%xcc,	%o5,	%i1
	ldx	[%i7+15*32],%o5
	movneg	%xcc,	%o4,	%i0
	add	%i7,16*32,	%i7
	movneg	%xcc,	%o5,	%i1
	ldx	[%i7+0*32],	%i2
	ldx	[%i7+8*32],	%i3
	ldx	[%i7+1*32],	%o4
	ldx	[%i7+9*32],	%o5
	movvs	%icc,	%o4,	%i2
	ldx	[%i7+2*32],	%o4
	movvs	%icc,	%o5,	%i3
	ldx	[%i7+10*32],%o5
	move	%icc,	%o4,	%i2
	ldx	[%i7+3*32],	%o4
	move	%icc,	%o5,	%i3
	ldx	[%i7+11*32],%o5
	movneg	%icc,	%o4,	%i2
	ldx	[%i7+4*32],	%o4
	movneg	%icc,	%o5,	%i3
	ldx	[%i7+12*32],%o5
	movcs	%xcc,	%o4,	%i2
	ldx	[%i7+5*32],%o4
	movcs	%xcc,	%o5,	%i3
	ldx	[%i7+13*32],%o5
	movvs	%xcc,	%o4,	%i2
	ldx	[%i7+6*32],	%o4
	movvs	%xcc,	%o5,	%i3
	ldx	[%i7+14*32],%o5
	move	%xcc,	%o4,	%i2
	ldx	[%i7+7*32],	%o4
	move	%xcc,	%o5,	%i3
	ldx	[%i7+15*32],%o5
	movneg	%xcc,	%o4,	%i2
	add	%i7,16*32,	%i7
	movneg	%xcc,	%o5,	%i3
	ldx	[%i7+0*32],	%i4
	ldx	[%i7+8*32],	%i5
	ldx	[%i7+1*32],	%o4
	ldx	[%i7+9*32],	%o5
	movvs	%icc,	%o4,	%i4
	ldx	[%i7+2*32],	%o4
	movvs	%icc,	%o5,	%i5
	ldx	[%i7+10*32],%o5
	move	%icc,	%o4,	%i4
	ldx	[%i7+3*32],	%o4
	move	%icc,	%o5,	%i5
	ldx	[%i7+11*32],%o5
	movneg	%icc,	%o4,	%i4
	ldx	[%i7+4*32],	%o4
	movneg	%icc,	%o5,	%i5
	ldx	[%i7+12*32],%o5
	movcs	%xcc,	%o4,	%i4
	ldx	[%i7+5*32],%o4
	movcs	%xcc,	%o5,	%i5
	ldx	[%i7+13*32],%o5
	movvs	%xcc,	%o4,	%i4
	ldx	[%i7+6*32],	%o4
	movvs	%xcc,	%o5,	%i5
	ldx	[%i7+14*32],%o5
	move	%xcc,	%o4,	%i4
	ldx	[%i7+7*32],	%o4
	move	%xcc,	%o5,	%i5
	ldx	[%i7+15*32],%o5
	movneg	%xcc,	%o4,	%i4
	add	%i7,16*32,	%i7
	movneg	%xcc,	%o5,	%i5
	ldx	[%i7+0*32],	%l0
	ldx	[%i7+8*32],	%l1
	ldx	[%i7+1*32],	%o4
	ldx	[%i7+9*32],	%o5
	movvs	%icc,	%o4,	%l0
	ldx	[%i7+2*32],	%o4
	movvs	%icc,	%o5,	%l1
	ldx	[%i7+10*32],%o5
	move	%icc,	%o4,	%l0
	ldx	[%i7+3*32],	%o4
	move	%icc,	%o5,	%l1
	ldx	[%i7+11*32],%o5
	movneg	%icc,	%o4,	%l0
	ldx	[%i7+4*32],	%o4
	movneg	%icc,	%o5,	%l1
	ldx	[%i7+12*32],%o5
	movcs	%xcc,	%o4,	%l0
	ldx	[%i7+5*32],%o4
	movcs	%xcc,	%o5,	%l1
	ldx	[%i7+13*32],%o5
	movvs	%xcc,	%o4,	%l0
	ldx	[%i7+6*32],	%o4
	movvs	%xcc,	%o5,	%l1
	ldx	[%i7+14*32],%o5
	move	%xcc,	%o4,	%l0
	ldx	[%i7+7*32],	%o4
	move	%xcc,	%o5,	%l1
	ldx	[%i7+15*32],%o5
	movneg	%xcc,	%o4,	%l0
	add	%i7,16*32,	%i7
	movneg	%xcc,	%o5,	%l1
	ldx	[%i7+0*32],	%l2
	ldx	[%i7+8*32],	%l3
	ldx	[%i7+1*32],	%o4
	ldx	[%i7+9*32],	%o5
	movvs	%icc,	%o4,	%l2
	ldx	[%i7+2*32],	%o4
	movvs	%icc,	%o5,	%l3
	ldx	[%i7+10*32],%o5
	move	%icc,	%o4,	%l2
	ldx	[%i7+3*32],	%o4
	move	%icc,	%o5,	%l3
	ldx	[%i7+11*32],%o5
	movneg	%icc,	%o4,	%l2
	ldx	[%i7+4*32],	%o4
	movneg	%icc,	%o5,	%l3
	ldx	[%i7+12*32],%o5
	movcs	%xcc,	%o4,	%l2
	ldx	[%i7+5*32],%o4
	movcs	%xcc,	%o5,	%l3
	ldx	[%i7+13*32],%o5
	movvs	%xcc,	%o4,	%l2
	ldx	[%i7+6*32],	%o4
	movvs	%xcc,	%o5,	%l3
	ldx	[%i7+14*32],%o5
	move	%xcc,	%o4,	%l2
	ldx	[%i7+7*32],	%o4
	move	%xcc,	%o5,	%l3
	ldx	[%i7+15*32],%o5
	movneg	%xcc,	%o4,	%l2
	add	%i7,16*32,	%i7
	movneg	%xcc,	%o5,	%l3
	ldx	[%i7+0*32],	%l4
	ldx	[%i7+8*32],	%l5
	ldx	[%i7+1*32],	%o4
	ldx	[%i7+9*32],	%o5
	movvs	%icc,	%o4,	%l4
	ldx	[%i7+2*32],	%o4
	movvs	%icc,	%o5,	%l5
	ldx	[%i7+10*32],%o5
	move	%icc,	%o4,	%l4
	ldx	[%i7+3*32],	%o4
	move	%icc,	%o5,	%l5
	ldx	[%i7+11*32],%o5
	movneg	%icc,	%o4,	%l4
	ldx	[%i7+4*32],	%o4
	movneg	%icc,	%o5,	%l5
	ldx	[%i7+12*32],%o5
	movcs	%xcc,	%o4,	%l4
	ldx	[%i7+5*32],%o4
	movcs	%xcc,	%o5,	%l5
	ldx	[%i7+13*32],%o5
	movvs	%xcc,	%o4,	%l4
	ldx	[%i7+6*32],	%o4
	movvs	%xcc,	%o5,	%l5
	ldx	[%i7+14*32],%o5
	move	%xcc,	%o4,	%l4
	ldx	[%i7+7*32],	%o4
	move	%xcc,	%o5,	%l5
	ldx	[%i7+15*32],%o5
	movneg	%xcc,	%o4,	%l4
	add	%i7,16*32,	%i7
	movneg	%xcc,	%o5,	%l5
	ldx	[%i7+0*32],	%l6
	ldx	[%i7+8*32],	%l7
	ldx	[%i7+1*32],	%o4
	ldx	[%i7+9*32],	%o5
	movvs	%icc,	%o4,	%l6
	ldx	[%i7+2*32],	%o4
	movvs	%icc,	%o5,	%l7
	ldx	[%i7+10*32],%o5
	move	%icc,	%o4,	%l6
	ldx	[%i7+3*32],	%o4
	move	%icc,	%o5,	%l7
	ldx	[%i7+11*32],%o5
	movneg	%icc,	%o4,	%l6
	ldx	[%i7+4*32],	%o4
	movneg	%icc,	%o5,	%l7
	ldx	[%i7+12*32],%o5
	movcs	%xcc,	%o4,	%l6
	ldx	[%i7+5*32],%o4
	movcs	%xcc,	%o5,	%l7
	ldx	[%i7+13*32],%o5
	movvs	%xcc,	%o4,	%l6
	ldx	[%i7+6*32],	%o4
	movvs	%xcc,	%o5,	%l7
	ldx	[%i7+14*32],%o5
	move	%xcc,	%o4,	%l6
	ldx	[%i7+7*32],	%o4
	move	%xcc,	%o5,	%l7
	ldx	[%i7+15*32],%o5
	movneg	%xcc,	%o4,	%l6
	add	%i7,16*32,	%i7
	movneg	%xcc,	%o5,	%l7
	ldx	[%i7+0*32],	%o0
	ldx	[%i7+8*32],	%o1
	ldx	[%i7+1*32],	%o4
	ldx	[%i7+9*32],	%o5
	movvs	%icc,	%o4,	%o0
	ldx	[%i7+2*32],	%o4
	movvs	%icc,	%o5,	%o1
	ldx	[%i7+10*32],%o5
	move	%icc,	%o4,	%o0
	ldx	[%i7+3*32],	%o4
	move	%icc,	%o5,	%o1
	ldx	[%i7+11*32],%o5
	movneg	%icc,	%o4,	%o0
	ldx	[%i7+4*32],	%o4
	movneg	%icc,	%o5,	%o1
	ldx	[%i7+12*32],%o5
	movcs	%xcc,	%o4,	%o0
	ldx	[%i7+5*32],%o4
	movcs	%xcc,	%o5,	%o1
	ldx	[%i7+13*32],%o5
	movvs	%xcc,	%o4,	%o0
	ldx	[%i7+6*32],	%o4
	movvs	%xcc,	%o5,	%o1
	ldx	[%i7+14*32],%o5
	move	%xcc,	%o4,	%o0
	ldx	[%i7+7*32],	%o4
	move	%xcc,	%o5,	%o1
	ldx	[%i7+15*32],%o5
	movneg	%xcc,	%o4,	%o0
	add	%i7,16*32,	%i7
	movneg	%xcc,	%o5,	%o1
	ldx	[%i7+0*32],	%o2
	ldx	[%i7+8*32],	%o3
	ldx	[%i7+1*32],	%o4
	ldx	[%i7+9*32],	%o5
	movvs	%icc,	%o4,	%o2
	ldx	[%i7+2*32],	%o4
	movvs	%icc,	%o5,	%o3
	ldx	[%i7+10*32],%o5
	move	%icc,	%o4,	%o2
	ldx	[%i7+3*32],	%o4
	move	%icc,	%o5,	%o3
	ldx	[%i7+11*32],%o5
	movneg	%icc,	%o4,	%o2
	ldx	[%i7+4*32],	%o4
	movneg	%icc,	%o5,	%o3
	ldx	[%i7+12*32],%o5
	movcs	%xcc,	%o4,	%o2
	ldx	[%i7+5*32],%o4
	movcs	%xcc,	%o5,	%o3
	ldx	[%i7+13*32],%o5
	movvs	%xcc,	%o4,	%o2
	ldx	[%i7+6*32],	%o4
	movvs	%xcc,	%o5,	%o3
	ldx	[%i7+14*32],%o5
	move	%xcc,	%o4,	%o2
	ldx	[%i7+7*32],	%o4
	move	%xcc,	%o5,	%o3
	ldx	[%i7+15*32],%o5
	movneg	%xcc,	%o4,	%o2
	add	%i7,16*32,	%i7
	movneg	%xcc,	%o5,	%o3
	srax	%g4,	32,	%o4		! unpack %g4
	srl	%g4,	%g0,	%o5
	sub	%o4,	5,	%o4
	mov	%g3,	%i7
	sllx	%o4,	32,	%g4		! re-pack %g4
	or	%o5,	%g4,	%g4
	srl	%o5,	%o4,	%o5
	srl	%o5,	2,	%o4
	and	%o5,	3,	%o5
	and	%o4,	7,	%o4
	sll	%o5,	3,	%o5	! offset within first cache line
	add	%o5,	%i7,	%i7	! of the pwrtbl
	or	%g0,	1,	%o5
	sll	%o5,	%o4,	%o4
	.word	0x81b02940+32-1	! montsqr	32-1
	fbu,pn	%fcc3,.Labort_32
#ifndef	__arch64__
	and	%fp,%g5,%g5
	brz,pn	%g5,.Labort_32
#endif
	nop
	.word	0x81b02940+32-1	! montsqr	32-1
	fbu,pn	%fcc3,.Labort_32
#ifndef	__arch64__
	and	%fp,%g5,%g5
	brz,pn	%g5,.Labort_32
#endif
	nop
	.word	0x81b02940+32-1	! montsqr	32-1
	fbu,pn	%fcc3,.Labort_32
#ifndef	__arch64__
	and	%fp,%g5,%g5
	brz,pn	%g5,.Labort_32
#endif
	nop
	.word	0x81b02940+32-1	! montsqr	32-1
	fbu,pn	%fcc3,.Labort_32
#ifndef	__arch64__
	and	%fp,%g5,%g5
	brz,pn	%g5,.Labort_32
#endif
	nop
	.word	0x81b02940+32-1	! montsqr	32-1
	fbu,pn	%fcc3,.Labort_32
#ifndef	__arch64__
	and	%fp,%g5,%g5
	brz,pn	%g5,.Labort_32
#endif
	nop
	wr	%o4,	%g0,	%ccr
	.word	0x81b02920+32-1	! montmul	32-1
	fbu,pn	%fcc3,.Labort_32
#ifndef	__arch64__
	and	%fp,%g5,%g5
	brz,pn	%g5,.Labort_32
#endif

	srax	%g4,	32,	%o4
#ifdef	__arch64__
	brgez	%o4,.Lstride_32
	restore
	restore
	restore
	restore
	restore
#else
	brgez	%o4,.Lstride_32
	restore;		and	%fp,%g5,%g5
	restore;		and	%fp,%g5,%g5
	restore;		and	%fp,%g5,%g5
	restore;		and	%fp,%g5,%g5
	 brz,pn	%g5,.Labort1_32
	restore
#endif
	.word	0x81b02310 !movxtod	%l0,%f0
	.word	0x85b02311 !movxtod	%l1,%f2
	.word	0x89b02312 !movxtod	%l2,%f4
	.word	0x8db02313 !movxtod	%l3,%f6
	.word	0x91b02314 !movxtod	%l4,%f8
	.word	0x95b02315 !movxtod	%l5,%f10
	.word	0x99b02316 !movxtod	%l6,%f12
	.word	0x9db02317 !movxtod	%l7,%f14
	.word	0xa1b02308 !movxtod	%o0,%f16
	.word	0xa5b02309 !movxtod	%o1,%f18
	.word	0xa9b0230a !movxtod	%o2,%f20
	.word	0xadb0230b !movxtod	%o3,%f22
	.word	0xbbb0230c !movxtod	%o4,%f60
	.word	0xbfb0230d !movxtod	%o5,%f62
#ifdef	__arch64__
	restore
#else
	 and	%fp,%g5,%g5
	restore
	 and	%g5,1,%o7
	 and	%fp,%g5,%g5
	 srl	%fp,0,%fp		! just in case?
	 or	%o7,%g5,%g5
	brz,a,pn %g5,.Ldone_32
	mov	0,%i0		! return failure
#endif
	std	%f0,[%g1+0*8]
	std	%f2,[%g1+1*8]
	std	%f4,[%g1+2*8]
	std	%f6,[%g1+3*8]
	std	%f8,[%g1+4*8]
	std	%f10,[%g1+5*8]
	std	%f12,[%g1+6*8]
	std	%f14,[%g1+7*8]
	std	%f16,[%g1+8*8]
	std	%f18,[%g1+9*8]
	std	%f20,[%g1+10*8]
	std	%f22,[%g1+11*8]
	std	%f60,[%g1+12*8]
	std	%f62,[%g1+13*8]
	std	%f24,[%g1+14*8]
	std	%f26,[%g1+15*8]
	std	%f28,[%g1+16*8]
	std	%f30,[%g1+17*8]
	std	%f32,[%g1+18*8]
	std	%f34,[%g1+19*8]
	std	%f36,[%g1+20*8]
	std	%f38,[%g1+21*8]
	std	%f40,[%g1+22*8]
	std	%f42,[%g1+23*8]
	std	%f44,[%g1+24*8]
	std	%f46,[%g1+25*8]
	std	%f48,[%g1+26*8]
	std	%f50,[%g1+27*8]
	std	%f52,[%g1+28*8]
	std	%f54,[%g1+29*8]
	std	%f56,[%g1+30*8]
	std	%f58,[%g1+31*8]
	mov	1,%i0		! return success
.Ldone_32:
	ret
	restore

.Labort_32:
	restore
	restore
	restore
	restore
	restore
.Labort1_32:
	restore

	mov	0,%i0		! return failure
	ret
	restore
.type	bn_pwr5_mont_t4_32, #function
.size	bn_pwr5_mont_t4_32, .-bn_pwr5_mont_t4_32
.globl	bn_mul_mont_t4
.align	32
bn_mul_mont_t4:
	add	%sp,	STACK_BIAS,	%g4	! real top of stack
	sll	%o5,	3,	%o5		! size in bytes
	add	%o5,	63,	%g1
	andn	%g1,	63,	%g1		! buffer size rounded up to 64 bytes
	sub	%g4,	%g1,	%g1
	andn	%g1,	63,	%g1		! align at 64 byte
	sub	%g1,	STACK_FRAME,	%g1	! new top of stack
	sub	%g1,	%g4,	%g1

	save	%sp,	%g1,	%sp
	ld	[%i4+0],	%l0	! pull n0[0..1] value
	ld	[%i4+4],	%l1
	add	%sp, STACK_BIAS+STACK_FRAME, %l5
	ldx	[%i2+0],	%g2	! m0=bp[0]
	sllx	%l1,	32,	%g1
	add	%i2,	8,	%i2
	or	%l0,	%g1,	%g1

	ldx	[%i1+0],	%o2	! ap[0]

	mulx	%o2,	%g2,	%g4	! ap[0]*bp[0]
	.word	0x8bb282c2 !umulxhi	%o2,%g2,%g5

	ldx	[%i1+8],	%o2	! ap[1]
	add	%i1,	16,	%i1
	ldx	[%i3+0],	%o4	! np[0]

	mulx	%g4,	%g1,	%g3	! "tp[0]"*n0

	mulx	%o2,	%g2,	%o3	! ap[1]*bp[0]
	.word	0x95b282c2 !umulxhi	%o2,%g2,%o2	! ahi=aj

	mulx	%o4,	%g3,	%o0	! np[0]*m1
	.word	0x93b302c3 !umulxhi	%o4,%g3,%o1

	ldx	[%i3+8],	%o4	! np[1]

	addcc	%g4,	%o0,	%o0
	add	%i3,	16,	%i3
	.word	0x93b00229 !addxc	%g0,%o1,%o1

	mulx	%o4,	%g3,	%o5	! np[1]*m1
	.word	0x99b302c3 !umulxhi	%o4,%g3,%o4	! nhi=nj

	ba	.L1st
	sub	%i5,	24,	%l4	! cnt=num-3

.align	16
.L1st:
	addcc	%o3,	%g5,	%g4
	.word	0x8bb28220 !addxc	%o2,%g0,%g5

	ldx	[%i1+0],	%o2	! ap[j]
	addcc	%o5,	%o1,	%o0
	add	%i1,	8,	%i1
	.word	0x93b30220 !addxc	%o4,%g0,%o1	! nhi=nj

	ldx	[%i3+0],	%o4	! np[j]
	mulx	%o2,	%g2,	%o3	! ap[j]*bp[0]
	add	%i3,	8,	%i3
	.word	0x95b282c2 !umulxhi	%o2,%g2,%o2	! ahi=aj

	mulx	%o4,	%g3,	%o5	! np[j]*m1
	addcc	%g4,	%o0,	%o0	! np[j]*m1+ap[j]*bp[0]
	.word	0x99b302c3 !umulxhi	%o4,%g3,%o4	! nhi=nj
	.word	0x93b00229 !addxc	%g0,%o1,%o1
	stxa	%o0,	[%l5]0xe2	! tp[j-1]
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
	stxa	%o0,	[%l5]0xe2	! tp[j-1]
	add	%l5,	8,	%l5

	addcc	%g5,	%o1,	%o1
	.word	0xa1b00220 !addxc	%g0,%g0,%l0	! upmost overflow bit
	stxa	%o1,	[%l5]0xe2
	add	%l5,	8,	%l5

	ba	.Louter
	sub	%i5,	16,	%l1	! i=num-2

.align	16
.Louter:
	ldx	[%i2+0],	%g2	! m0=bp[i]
	add	%i2,	8,	%i2

	sub	%i1,	%i5,	%i1	! rewind
	sub	%i3,	%i5,	%i3
	sub	%l5,	%i5,	%l5

	ldx	[%i1+0],	%o2	! ap[0]
	ldx	[%i3+0],	%o4	! np[0]

	mulx	%o2,	%g2,	%g4	! ap[0]*bp[i]
	ldx	[%l5],		%o7	! tp[0]
	.word	0x8bb282c2 !umulxhi	%o2,%g2,%g5
	ldx	[%i1+8],	%o2	! ap[1]
	addcc	%g4,	%o7,	%g4	! ap[0]*bp[i]+tp[0]
	mulx	%o2,	%g2,	%o3	! ap[1]*bp[i]
	.word	0x8bb00225 !addxc	%g0,%g5,%g5
	mulx	%g4,	%g1,	%g3	! tp[0]*n0
	.word	0x95b282c2 !umulxhi	%o2,%g2,%o2	! ahi=aj
	mulx	%o4,	%g3,	%o0	! np[0]*m1
	add	%i1,	16,	%i1
	.word	0x93b302c3 !umulxhi	%o4,%g3,%o1
	ldx	[%i3+8],	%o4	! np[1]
	add	%i3,	16,	%i3
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
	ldx	[%i1+0],	%o2	! ap[j]
	add	%i1,	8,	%i1
	addcc	%o5,	%o1,	%o0
	mulx	%o2,	%g2,	%o3	! ap[j]*bp[i]
	.word	0x93b30220 !addxc	%o4,%g0,%o1	! nhi=nj
	ldx	[%i3+0],	%o4	! np[j]
	add	%i3,	8,	%i3
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

	sub	%i1,	%i5,	%i1	! rewind
	sub	%i3,	%i5,	%i3
	sub	%l5,	%i5,	%l5
	ba	.Lsub
	subcc	%i5,	8,	%l4	! cnt=num-1 and clear CCR.xcc

.align	16
.Lsub:
	ldx	[%l5],		%o7
	add	%l5,	8,	%l5
	ldx	[%i3+0],	%o4
	add	%i3,	8,	%i3
	subccc	%o7,	%o4,	%l2	! tp[j]-np[j]
	srlx	%o7,	32,	%o7
	srlx	%o4,	32,	%o4
	subccc	%o7,	%o4,	%l3
	add	%i0,	8,	%i0
	st	%l2,	[%i0-4]		! reverse order
	st	%l3,	[%i0-8]
	brnz,pt	%l4,	.Lsub
	sub	%l4,	8,	%l4

	sub	%i3,	%i5,	%i3	! rewind
	sub	%l5,	%i5,	%l5
	sub	%i0,	%i5,	%i0

	subc	%l0,	%g0,	%l0	! handle upmost overflow bit
	and	%l5,	%l0,	%i1
	andn	%i0,	%l0,	%i3
	or	%i3,	%i1,	%i1	! ap=borrow?tp:rp
	ba	.Lcopy
	sub	%i5,	8,	%l4

.align	16
.Lcopy:					! copy or in-place refresh
	ldx	[%i1+0],	%l2
	add	%i1,	8,	%i1
	stx	%g0,	[%l5]		! zap
	add	%l5,	8,	%l5
	stx	%l2,	[%i0+0]
	add	%i0,	8,	%i0
	brnz	%l4,	.Lcopy
	sub	%l4,	8,	%l4

	mov	1,	%o0
	ret
	restore
.type	bn_mul_mont_t4, #function
.size	bn_mul_mont_t4, .-bn_mul_mont_t4
.globl	bn_mul_mont_gather5_t4
.align	32
bn_mul_mont_gather5_t4:
	add	%sp,	STACK_BIAS,	%g4	! real top of stack
	sll	%o5,	3,	%o5		! size in bytes
	add	%o5,	63,	%g1
	andn	%g1,	63,	%g1		! buffer size rounded up to 64 bytes
	sub	%g4,	%g1,	%g1
	andn	%g1,	63,	%g1		! align at 64 byte
	sub	%g1,	STACK_FRAME,	%g1	! new top of stack
	sub	%g1,	%g4,	%g1
	LDPTR	[%sp+STACK_7thARG],	%g4	! load power, 7th argument

	save	%sp,	%g1,	%sp
	srl	%g4,	2,	%o4
	and	%g4,	3,	%o5
	and	%o4,	7,	%o4
	sll	%o5,	3,	%o5	! offset within first cache line
	add	%o5,	%i2,	%i2	! of the pwrtbl
	or	%g0,	1,	%o5
	sll	%o5,	%o4,	%l7
	wr	%l7,	%g0,	%ccr
	ldx	[%i2+0*32],	%g2
	ldx	[%i2+1*32],	%o4
	ldx	[%i2+2*32],	%o5
	movvs	%icc,	%o4,	%g2
	ldx	[%i2+3*32],	%o4
	move	%icc,	%o5,	%g2
	ldx	[%i2+4*32],	%o5
	movneg	%icc,	%o4,	%g2
	ldx	[%i2+5*32],	%o4
	movcs	%xcc,	%o5,	%g2
	ldx	[%i2+6*32],	%o5
	movvs	%xcc,	%o4,	%g2
	ldx	[%i2+7*32],	%o4
	move	%xcc,	%o5,	%g2
	add	%i2,8*32,	%i2
	movneg	%xcc,	%o4,	%g2
	ld	[%i4+0],	%l0	! pull n0[0..1] value
	ld	[%i4+4],	%l1
	add	%sp, STACK_BIAS+STACK_FRAME, %l5
	sllx	%l1,	32,	%g1
	or	%l0,	%g1,	%g1

	ldx	[%i1+0],	%o2	! ap[0]

	mulx	%o2,	%g2,	%g4	! ap[0]*bp[0]
	.word	0x8bb282c2 !umulxhi	%o2,%g2,%g5

	ldx	[%i1+8],	%o2	! ap[1]
	add	%i1,	16,	%i1
	ldx	[%i3+0],	%o4	! np[0]

	mulx	%g4,	%g1,	%g3	! "tp[0]"*n0

	mulx	%o2,	%g2,	%o3	! ap[1]*bp[0]
	.word	0x95b282c2 !umulxhi	%o2,%g2,%o2	! ahi=aj

	mulx	%o4,	%g3,	%o0	! np[0]*m1
	.word	0x93b302c3 !umulxhi	%o4,%g3,%o1

	ldx	[%i3+8],	%o4	! np[1]

	addcc	%g4,	%o0,	%o0
	add	%i3,	16,	%i3
	.word	0x93b00229 !addxc	%g0,%o1,%o1

	mulx	%o4,	%g3,	%o5	! np[1]*m1
	.word	0x99b302c3 !umulxhi	%o4,%g3,%o4	! nhi=nj

	ba	.L1st_g5
	sub	%i5,	24,	%l4	! cnt=num-3

.align	16
.L1st_g5:
	addcc	%o3,	%g5,	%g4
	.word	0x8bb28220 !addxc	%o2,%g0,%g5

	ldx	[%i1+0],	%o2	! ap[j]
	addcc	%o5,	%o1,	%o0
	add	%i1,	8,	%i1
	.word	0x93b30220 !addxc	%o4,%g0,%o1	! nhi=nj

	ldx	[%i3+0],	%o4	! np[j]
	mulx	%o2,	%g2,	%o3	! ap[j]*bp[0]
	add	%i3,	8,	%i3
	.word	0x95b282c2 !umulxhi	%o2,%g2,%o2	! ahi=aj

	mulx	%o4,	%g3,	%o5	! np[j]*m1
	addcc	%g4,	%o0,	%o0	! np[j]*m1+ap[j]*bp[0]
	.word	0x99b302c3 !umulxhi	%o4,%g3,%o4	! nhi=nj
	.word	0x93b00229 !addxc	%g0,%o1,%o1
	stxa	%o0,	[%l5]0xe2	! tp[j-1]
	add	%l5,	8,	%l5	! tp++

	brnz,pt	%l4,	.L1st_g5
	sub	%l4,	8,	%l4	! j--
!.L1st_g5
	addcc	%o3,	%g5,	%g4
	.word	0x8bb28220 !addxc	%o2,%g0,%g5	! ahi=aj

	addcc	%o5,	%o1,	%o0
	.word	0x93b30220 !addxc	%o4,%g0,%o1
	addcc	%g4,	%o0,	%o0	! np[j]*m1+ap[j]*bp[0]
	.word	0x93b00229 !addxc	%g0,%o1,%o1
	stxa	%o0,	[%l5]0xe2	! tp[j-1]
	add	%l5,	8,	%l5

	addcc	%g5,	%o1,	%o1
	.word	0xa1b00220 !addxc	%g0,%g0,%l0	! upmost overflow bit
	stxa	%o1,	[%l5]0xe2
	add	%l5,	8,	%l5

	ba	.Louter_g5
	sub	%i5,	16,	%l1	! i=num-2

.align	16
.Louter_g5:
	wr	%l7,	%g0,	%ccr
	ldx	[%i2+0*32],	%g2
	ldx	[%i2+1*32],	%o4
	ldx	[%i2+2*32],	%o5
	movvs	%icc,	%o4,	%g2
	ldx	[%i2+3*32],	%o4
	move	%icc,	%o5,	%g2
	ldx	[%i2+4*32],	%o5
	movneg	%icc,	%o4,	%g2
	ldx	[%i2+5*32],	%o4
	movcs	%xcc,	%o5,	%g2
	ldx	[%i2+6*32],	%o5
	movvs	%xcc,	%o4,	%g2
	ldx	[%i2+7*32],	%o4
	move	%xcc,	%o5,	%g2
	add	%i2,8*32,	%i2
	movneg	%xcc,	%o4,	%g2
	sub	%i1,	%i5,	%i1	! rewind
	sub	%i3,	%i5,	%i3
	sub	%l5,	%i5,	%l5

	ldx	[%i1+0],	%o2	! ap[0]
	ldx	[%i3+0],	%o4	! np[0]

	mulx	%o2,	%g2,	%g4	! ap[0]*bp[i]
	ldx	[%l5],		%o7	! tp[0]
	.word	0x8bb282c2 !umulxhi	%o2,%g2,%g5
	ldx	[%i1+8],	%o2	! ap[1]
	addcc	%g4,	%o7,	%g4	! ap[0]*bp[i]+tp[0]
	mulx	%o2,	%g2,	%o3	! ap[1]*bp[i]
	.word	0x8bb00225 !addxc	%g0,%g5,%g5
	mulx	%g4,	%g1,	%g3	! tp[0]*n0
	.word	0x95b282c2 !umulxhi	%o2,%g2,%o2	! ahi=aj
	mulx	%o4,	%g3,	%o0	! np[0]*m1
	add	%i1,	16,	%i1
	.word	0x93b302c3 !umulxhi	%o4,%g3,%o1
	ldx	[%i3+8],	%o4	! np[1]
	add	%i3,	16,	%i3
	addcc	%o0,	%g4,	%o0
	mulx	%o4,	%g3,	%o5	! np[1]*m1
	.word	0x93b00229 !addxc	%g0,%o1,%o1
	.word	0x99b302c3 !umulxhi	%o4,%g3,%o4	! nhi=nj

	ba	.Linner_g5
	sub	%i5,	24,	%l4	! cnt=num-3
.align	16
.Linner_g5:
	addcc	%o3,	%g5,	%g4
	ldx	[%l5+8],	%o7	! tp[j]
	.word	0x8bb28220 !addxc	%o2,%g0,%g5	! ahi=aj
	ldx	[%i1+0],	%o2	! ap[j]
	add	%i1,	8,	%i1
	addcc	%o5,	%o1,	%o0
	mulx	%o2,	%g2,	%o3	! ap[j]*bp[i]
	.word	0x93b30220 !addxc	%o4,%g0,%o1	! nhi=nj
	ldx	[%i3+0],	%o4	! np[j]
	add	%i3,	8,	%i3
	.word	0x95b282c2 !umulxhi	%o2,%g2,%o2	! ahi=aj
	addcc	%g4,	%o7,	%g4	! ap[j]*bp[i]+tp[j]
	mulx	%o4,	%g3,	%o5	! np[j]*m1
	.word	0x8bb00225 !addxc	%g0,%g5,%g5
	.word	0x99b302c3 !umulxhi	%o4,%g3,%o4	! nhi=nj
	addcc	%o0,	%g4,	%o0	! np[j]*m1+ap[j]*bp[i]+tp[j]
	.word	0x93b00229 !addxc	%g0,%o1,%o1
	stx	%o0,	[%l5]		! tp[j-1]
	add	%l5,	8,	%l5
	brnz,pt	%l4,	.Linner_g5
	sub	%l4,	8,	%l4
!.Linner_g5
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

	brnz,pt	%l1,	.Louter_g5
	sub	%l1,	8,	%l1

	sub	%i1,	%i5,	%i1	! rewind
	sub	%i3,	%i5,	%i3
	sub	%l5,	%i5,	%l5
	ba	.Lsub_g5
	subcc	%i5,	8,	%l4	! cnt=num-1 and clear CCR.xcc

.align	16
.Lsub_g5:
	ldx	[%l5],		%o7
	add	%l5,	8,	%l5
	ldx	[%i3+0],	%o4
	add	%i3,	8,	%i3
	subccc	%o7,	%o4,	%l2	! tp[j]-np[j]
	srlx	%o7,	32,	%o7
	srlx	%o4,	32,	%o4
	subccc	%o7,	%o4,	%l3
	add	%i0,	8,	%i0
	st	%l2,	[%i0-4]		! reverse order
	st	%l3,	[%i0-8]
	brnz,pt	%l4,	.Lsub_g5
	sub	%l4,	8,	%l4

	sub	%i3,	%i5,	%i3	! rewind
	sub	%l5,	%i5,	%l5
	sub	%i0,	%i5,	%i0

	subc	%l0,	%g0,	%l0	! handle upmost overflow bit
	and	%l5,	%l0,	%i1
	andn	%i0,	%l0,	%i3
	or	%i3,	%i1,	%i1	! ap=borrow?tp:rp
	ba	.Lcopy_g5
	sub	%i5,	8,	%l4

.align	16
.Lcopy_g5:				! copy or in-place refresh
	ldx	[%i1+0],	%l2
	add	%i1,	8,	%i1
	stx	%g0,	[%l5]		! zap
	add	%l5,	8,	%l5
	stx	%l2,	[%i0+0]
	add	%i0,	8,	%i0
	brnz	%l4,	.Lcopy_g5
	sub	%l4,	8,	%l4

	mov	1,	%o0
	ret
	restore
.type	bn_mul_mont_gather5_t4, #function
.size	bn_mul_mont_gather5_t4, .-bn_mul_mont_gather5_t4
.globl	bn_flip_t4
.align	32
bn_flip_t4:
.Loop_flip:
	ld	[%o1+0],	%o4
	sub	%o2,	1,	%o2
	ld	[%o1+4],	%o5
	add	%o1,	8,	%o1
	st	%o5,	[%o0+0]
	st	%o4,	[%o0+4]
	brnz	%o2,	.Loop_flip
	add	%o0,	8,	%o0
	retl
	nop
.type	bn_flip_t4, #function
.size	bn_flip_t4, .-bn_flip_t4

.globl	bn_flip_n_scatter5_t4
.align	32
bn_flip_n_scatter5_t4:
	sll	%o3,	3,	%o3
	srl	%o1,	1,	%o1
	add	%o3,	%o2,	%o2	! &pwrtbl[pwr]
	sub	%o1,	1,	%o1
.Loop_flip_n_scatter5:
	ld	[%o0+0],	%o4	! inp[i]
	ld	[%o0+4],	%o5
	add	%o0,	8,	%o0
	sllx	%o5,	32,	%o5
	or	%o4,	%o5,	%o5
	stx	%o5,	[%o2]
	add	%o2,	32*8,	%o2
	brnz	%o1,	.Loop_flip_n_scatter5
	sub	%o1,	1,	%o1
	retl
	nop
.type	bn_flip_n_scatter5_t4, #function
.size	bn_flip_n_scatter5_t4, .-bn_flip_n_scatter5_t4

.globl	bn_gather5_t4
.align	32
bn_gather5_t4:
	srl	%o3,	2,	%o4
	and	%o3,	3,	%o5
	and	%o4,	7,	%o4
	sll	%o5,	3,	%o5	! offset within first cache line
	add	%o5,	%o2,	%o2	! of the pwrtbl
	or	%g0,	1,	%o5
	sll	%o5,	%o4,	%g1
	wr	%g1,	%g0,	%ccr
	sub	%o1,	1,	%o1
.Loop_gather5:
	ldx	[%o2+0*32],	%g1
	ldx	[%o2+1*32],	%o4
	ldx	[%o2+2*32],	%o5
	movvs	%icc,	%o4,	%g1
	ldx	[%o2+3*32],	%o4
	move	%icc,	%o5,	%g1
	ldx	[%o2+4*32],	%o5
	movneg	%icc,	%o4,	%g1
	ldx	[%o2+5*32],	%o4
	movcs	%xcc,	%o5,	%g1
	ldx	[%o2+6*32],	%o5
	movvs	%xcc,	%o4,	%g1
	ldx	[%o2+7*32],	%o4
	move	%xcc,	%o5,	%g1
	add	%o2,8*32,	%o2
	movneg	%xcc,	%o4,	%g1
	stx	%g1,	[%o0]
	add	%o0,	8,	%o0
	brnz	%o1,	.Loop_gather5
	sub	%o1,	1,	%o1

	retl
	nop
.type	bn_gather5_t4, #function
.size	bn_gather5_t4, .-bn_gather5_t4

.asciz	"Montgomery Multiplication for SPARC T4, David S. Miller, Andy Polyakov"
.align	4
