#include <sparc_arch.h>

#ifdef __arch64__
.register	%g2,#scratch
.register	%g3,#scratch
#endif

#ifdef __PIC__
SPARC_PIC_THUNK(%g1)
#endif

.globl	bn_GF2m_mul_2x2
.align	16
bn_GF2m_mul_2x2:
        SPARC_LOAD_ADDRESS_LEAF(OPENSSL_sparcv9cap_P,%g1,%g5)
        ld	[%g1+0],%g1             	! OPENSSL_sparcv9cap_P[0]

        andcc	%g1, SPARCV9_VIS3, %g0
        bz,pn	%icc,.Lsoftware
        nop

	sllx	%o1, 32, %o1
	sllx	%o3, 32, %o3
	or	%o2, %o1, %o1
	or	%o4, %o3, %o3
	.word	0x95b262ab			! xmulx   %o1, %o3, %o2
	.word	0x99b262cb			! xmulxhi %o1, %o3, %o4
	srlx	%o2, 32, %o1			! 13 cycles later
	st	%o2, [%o0+0]
	st	%o1, [%o0+4]
	srlx	%o4, 32, %o3
	st	%o4, [%o0+8]
	retl
	st	%o3, [%o0+12]

.align	16
.Lsoftware:
	save	%sp,-STACK_FRAME-128,%sp

	sllx	%i1,32,%g1
	mov	-1,%o4
	sllx	%i3,32,%o7
	or	%i2,%g1,%g1
	srlx	%o4,1,%o5			! 0x7fff...
	or	%i4,%o7,%o7
	srlx	%o4,2,%o4			! 0x3fff...
	add	%sp,STACK_BIAS+STACK_FRAME,%l0

	sllx	%g1,2,%o2
	mov	%g1,%o0
	sllx	%g1,1,%o1

	srax	%o2,63,%g5			! broadcast 61st bit
	and	%o5,%o2,%o2			! (a<<2)&0x7fff...
	srlx	%o5,2,%o5
	srax	%o1,63,%g4			! broadcast 62nd bit
	and	%o4,%o1,%o1			! (a<<1)&0x3fff...
	srax	%o0,63,%g1			! broadcast 63rd bit
	and	%o5,%o0,%o0			! (a<<0)&0x1fff...

	sllx	%o0,3,%o3
	and	%o7,%g1,%g1
	and	%o7,%g4,%g4
	and	%o7,%g5,%g5

	stx	%g0,[%l0+0*8]			! tab[0]=0
	xor	%o0,%o1,%o4
	stx	%o0,[%l0+1*8]			! tab[1]=a1
	stx	%o1,[%l0+2*8]			! tab[2]=a2
	 xor	%o2,%o3,%o5
	stx	%o4,[%l0+3*8]			! tab[3]=a1^a2
	 xor	%o2,%o0,%o0

	stx	%o2,[%l0+4*8]			! tab[4]=a4
	xor	%o2,%o1,%o1
	stx	%o0,[%l0+5*8]			! tab[5]=a1^a4
	xor	%o2,%o4,%o4
	stx	%o1,[%l0+6*8]			! tab[6]=a2^a4
	 xor	%o5,%o0,%o0
	stx	%o4,[%l0+7*8]			! tab[7]=a1^a2^a4
	 xor	%o5,%o1,%o1

	stx	%o3,[%l0+8*8]			! tab[8]=a8
	xor	%o5,%o4,%o4
	stx	%o0,[%l0+9*8]			! tab[9]=a1^a8
	 xor	%o2,%o0,%o0
	stx	%o1,[%l0+10*8]			! tab[10]=a2^a8
	 xor	%o2,%o1,%o1
	stx	%o4,[%l0+11*8]		! tab[11]=a1^a2^a8

	xor	%o2,%o4,%o4
	stx	%o5,[%l0+12*8]		! tab[12]=a4^a8
	 srlx	%g1,1,%o3
	stx	%o0,[%l0+13*8]			! tab[13]=a1^a4^a8
	 sllx	%g1,63,%g1
	stx	%o1,[%l0+14*8]			! tab[14]=a2^a4^a8
	 srlx	%g4,2,%g2
	stx	%o4,[%l0+15*8]		! tab[15]=a1^a2^a4^a8

	sllx	%g4,62,%o0
	 sllx	%o7,3,%g4
	srlx	%g5,3,%g3
	 and	%g4,120,%g4
	sllx	%g5,61,%o1
	 ldx	[%l0+%g4],%g4
	 srlx	%o7,4-3,%g5
	xor	%g2,%o3,%o3
	 and	%g5,120,%g5
	xor	%o0,%g1,%g1
	 ldx	[%l0+%g5],%g5
	xor	%g3,%o3,%o3

	xor	%g4,%g1,%g1
	srlx	%o7,8-3,%g4
	 xor	%o1,%g1,%g1
	and	%g4,120,%g4
	sllx	%g5,4,%g2
	ldx	[%l0+%g4],%g4
	srlx	%g5,60,%g3
	xor	%g2,%g1,%g1
	srlx	%o7,12-3,%g5
	xor	%g3,%o3,%o3
	and	%g5,120,%g5
	sllx	%g4,8,%g3
	ldx	[%l0+%g5],%g5
	srlx	%g4,56,%g2
	xor	%g3,%g1,%g1
	srlx	%o7,16-3,%g4
	xor	%g2,%o3,%o3
	and	%g4,120,%g4
	sllx	%g5,12,%g2
	ldx	[%l0+%g4],%g4
	srlx	%g5,52,%g3
	xor	%g2,%g1,%g1
	srlx	%o7,20-3,%g5
	xor	%g3,%o3,%o3
	and	%g5,120,%g5
	sllx	%g4,16,%g3
	ldx	[%l0+%g5],%g5
	srlx	%g4,48,%g2
	xor	%g3,%g1,%g1
	srlx	%o7,24-3,%g4
	xor	%g2,%o3,%o3
	and	%g4,120,%g4
	sllx	%g5,20,%g2
	ldx	[%l0+%g4],%g4
	srlx	%g5,44,%g3
	xor	%g2,%g1,%g1
	srlx	%o7,28-3,%g5
	xor	%g3,%o3,%o3
	and	%g5,120,%g5
	sllx	%g4,24,%g3
	ldx	[%l0+%g5],%g5
	srlx	%g4,40,%g2
	xor	%g3,%g1,%g1
	srlx	%o7,32-3,%g4
	xor	%g2,%o3,%o3
	and	%g4,120,%g4
	sllx	%g5,28,%g2
	ldx	[%l0+%g4],%g4
	srlx	%g5,36,%g3
	xor	%g2,%g1,%g1
	srlx	%o7,36-3,%g5
	xor	%g3,%o3,%o3
	and	%g5,120,%g5
	sllx	%g4,32,%g3
	ldx	[%l0+%g5],%g5
	srlx	%g4,32,%g2
	xor	%g3,%g1,%g1
	srlx	%o7,40-3,%g4
	xor	%g2,%o3,%o3
	and	%g4,120,%g4
	sllx	%g5,36,%g2
	ldx	[%l0+%g4],%g4
	srlx	%g5,28,%g3
	xor	%g2,%g1,%g1
	srlx	%o7,44-3,%g5
	xor	%g3,%o3,%o3
	and	%g5,120,%g5
	sllx	%g4,40,%g3
	ldx	[%l0+%g5],%g5
	srlx	%g4,24,%g2
	xor	%g3,%g1,%g1
	srlx	%o7,48-3,%g4
	xor	%g2,%o3,%o3
	and	%g4,120,%g4
	sllx	%g5,44,%g2
	ldx	[%l0+%g4],%g4
	srlx	%g5,20,%g3
	xor	%g2,%g1,%g1
	srlx	%o7,52-3,%g5
	xor	%g3,%o3,%o3
	and	%g5,120,%g5
	sllx	%g4,48,%g3
	ldx	[%l0+%g5],%g5
	srlx	%g4,16,%g2
	xor	%g3,%g1,%g1
	srlx	%o7,56-3,%g4
	xor	%g2,%o3,%o3
	and	%g4,120,%g4
	sllx	%g5,52,%g2
	ldx	[%l0+%g4],%g4
	srlx	%g5,12,%g3
	xor	%g2,%g1,%g1
	srlx	%o7,60-3,%g5
	xor	%g3,%o3,%o3
	and	%g5,120,%g5
	sllx	%g4,56,%g3
	ldx	[%l0+%g5],%g5
	srlx	%g4,8,%g2
	xor	%g3,%g1,%g1

	sllx	%g5,60,%g3
	 xor	%g2,%o3,%o3
	srlx	%g5,4,%g2
	xor	%g3,%g1,%g1
	xor	%g2,%o3,%o3

	srlx	%g1,32,%i1
	st	%g1,[%i0+0]
	st	%i1,[%i0+4]
	srlx	%o3,32,%i2
	st	%o3,[%i0+8]
	st	%i2,[%i0+12]

	ret
	restore
.type	bn_GF2m_mul_2x2,#function
.size	bn_GF2m_mul_2x2,.-bn_GF2m_mul_2x2
.asciz	"GF(2^m) Multiplication for SPARCv9, CRYPTOGAMS by <appro@openssl.org>"
.align	4
