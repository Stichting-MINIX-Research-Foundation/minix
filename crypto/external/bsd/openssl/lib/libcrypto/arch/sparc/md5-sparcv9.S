#include "sparc_arch.h"

#ifdef __arch64__
.register	%g2,#scratch
.register	%g3,#scratch
#endif

.section	".text",#alloc,#execinstr

#ifdef __PIC__
SPARC_PIC_THUNK(%g1)
#endif

.globl	md5_block_asm_data_order
.align	32
md5_block_asm_data_order:
	SPARC_LOAD_ADDRESS_LEAF(OPENSSL_sparcv9cap_P,%g1,%g5)
	ld	[%g1+4],%g1		! OPENSSL_sparcv9cap_P[1]

	andcc	%g1, CFR_MD5, %g0
	be	.Lsoftware
	nop

	mov	4, %g1
	andcc	%o1, 0x7, %g0
	lda	[%o0 + %g0]0x88, %f0		! load context
	lda	[%o0 + %g1]0x88, %f1
	add	%o0, 8, %o0
	lda	[%o0 + %g0]0x88, %f2
	lda	[%o0 + %g1]0x88, %f3
	bne,pn	%icc, .Lhwunaligned
	sub	%o0, 8, %o0

.Lhw_loop:
	ldd	[%o1 + 0x00], %f8
	ldd	[%o1 + 0x08], %f10
	ldd	[%o1 + 0x10], %f12
	ldd	[%o1 + 0x18], %f14
	ldd	[%o1 + 0x20], %f16
	ldd	[%o1 + 0x28], %f18
	ldd	[%o1 + 0x30], %f20
	subcc	%o2, 1, %o2		! done yet? 
	ldd	[%o1 + 0x38], %f22
	add	%o1, 0x40, %o1
	prefetch [%o1 + 63], 20

	.word	0x81b02800		! MD5

	bne,pt	SIZE_T_CC, .Lhw_loop
	nop

.Lhwfinish:
	sta	%f0, [%o0 + %g0]0x88	! store context
	sta	%f1, [%o0 + %g1]0x88
	add	%o0, 8, %o0
	sta	%f2, [%o0 + %g0]0x88
	sta	%f3, [%o0 + %g1]0x88
	retl
	nop

.align	8
.Lhwunaligned:
	.word	0x93b24300 !alignaddr	%o1,%g0,%o1

	ldd	[%o1 + 0x00], %f10
.Lhwunaligned_loop:
	ldd	[%o1 + 0x08], %f12
	ldd	[%o1 + 0x10], %f14
	ldd	[%o1 + 0x18], %f16
	ldd	[%o1 + 0x20], %f18
	ldd	[%o1 + 0x28], %f20
	ldd	[%o1 + 0x30], %f22
	ldd	[%o1 + 0x38], %f24
	subcc	%o2, 1, %o2		! done yet?
	ldd	[%o1 + 0x40], %f26
	add	%o1, 0x40, %o1
	prefetch [%o1 + 63], 20

	.word	0x91b2890c !faligndata	%f10,%f12,%f8
	.word	0x95b3090e !faligndata	%f12,%f14,%f10
	.word	0x99b38910 !faligndata	%f14,%f16,%f12
	.word	0x9db40912 !faligndata	%f16,%f18,%f14
	.word	0xa1b48914 !faligndata	%f18,%f20,%f16
	.word	0xa5b50916 !faligndata	%f20,%f22,%f18
	.word	0xa9b58918 !faligndata	%f22,%f24,%f20
	.word	0xadb6091a !faligndata	%f24,%f26,%f22

	.word	0x81b02800		! MD5

	bne,pt	SIZE_T_CC, .Lhwunaligned_loop
	.word	0x95b68f9a !for	%f26,%f26,%f10	! %f10=%f26

	ba	.Lhwfinish
	nop

.align	16
.Lsoftware:
	save	%sp,-STACK_FRAME,%sp

	rd	%asi,%l7
	wr	%g0,0x88,%asi		! ASI_PRIMARY_LITTLE
	and	%i1,7,%i3
	andn	%i1,7,%i1

	sll	%i3,3,%i3		! *=8
	mov	56,%i5
	ld	[%i0+0],%l0
	sub	%i5,%i3,%i5
	ld	[%i0+4],%l1
	and	%i5,32,%i4
	add	%i5,8,%i5
	ld	[%i0+8],%l2
	sub	%i5,%i4,%i5	! shr+shl1+shl2==64
	ld	[%i0+12],%l3
	nop

.Loop:
	 cmp	%i3,0			! was inp aligned?
	ldxa	[%i1+0]%asi,%o0	! load little-endian input
	ldxa	[%i1+8]%asi,%o1
	ldxa	[%i1+16]%asi,%o2
	ldxa	[%i1+24]%asi,%o3
	ldxa	[%i1+32]%asi,%o4
	 sllx	%l0,32,%g4		! pack A,B
	ldxa	[%i1+40]%asi,%o5
	 sllx	%l2,32,%g5		! pack C,D
	ldxa	[%i1+48]%asi,%o7
	 or	%l1,%g4,%g4
	ldxa	[%i1+56]%asi,%g1
	 or	%l3,%g5,%g5
	bnz,a,pn	%icc,.+8
	ldxa	[%i1+64]%asi,%g2

	srlx	%o0,%i3,%o0	! align X[0]
	sllx	%o1,%i4,%g3
	 sethi	%hi(3614090360),%l5
	sllx	%g3,%i5,%g3
	 or	%l5,%lo(3614090360),%l5
	or	%g3,%o0,%o0
	 xor	%l2,%l3,%l4
	 add	%o0,%l5,%l5		! X[0]+K[0]
	 srlx	%o0,32,%g3		! extract X[1]
	and	%l1,%l4,%l4		! round 0
	add	%l5,%l0,%l0
	xor	%l3,%l4,%l4
	 sethi	%hi(3905402710),%l5
	add	%l4,%l0,%l0
	 or	%l5,%lo(3905402710),%l5
	sll	%l0,7,%l6
	 add	%g3,%l5,%l5		! X[1]+K[1]
	srl	%l0,32-7,%l0
	add	%l1,%l6,%l6
	 xor	 %l1,%l2,%l4
	add	%l6,%l0,%l0
	 srlx	%o1,%i3,%o1	! align X[2]
	and	%l0,%l4,%l4		! round 1
	 sllx	%o2,%i4,%g3
	add	%l5,%l3,%l3
	 sllx	%g3,%i5,%g3
	xor	%l2,%l4,%l4
	 or	%g3,%o1,%o1
	 sethi	%hi(606105819),%l5
	add	%l4,%l3,%l3
	 or	%l5,%lo(606105819),%l5
	sll	%l3,12,%l6
	 add	%o1,%l5,%l5		! X[2]+K[2]
	srl	%l3,32-12,%l3
	add	%l0,%l6,%l6
	 xor	 %l0,%l1,%l4
	add	%l6,%l3,%l3
	 srlx	%o1,32,%g3		! extract X[3]
	and	%l3,%l4,%l4		! round 2
	add	%l5,%l2,%l2
	xor	%l1,%l4,%l4
	 sethi	%hi(3250441966),%l5
	add	%l4,%l2,%l2
	 or	%l5,%lo(3250441966),%l5
	sll	%l2,17,%l6
	 add	%g3,%l5,%l5		! X[3]+K[3]
	srl	%l2,32-17,%l2
	add	%l3,%l6,%l6
	 xor	 %l3,%l0,%l4
	add	%l6,%l2,%l2
	 srlx	%o2,%i3,%o2	! align X[4]
	and	%l2,%l4,%l4		! round 3
	 sllx	%o3,%i4,%g3
	add	%l5,%l1,%l1
	 sllx	%g3,%i5,%g3
	xor	%l0,%l4,%l4
	 or	%g3,%o2,%o2
	 sethi	%hi(4118548399),%l5
	add	%l4,%l1,%l1
	 or	%l5,%lo(4118548399),%l5
	sll	%l1,22,%l6
	 add	%o2,%l5,%l5		! X[4]+K[4]
	srl	%l1,32-22,%l1
	add	%l2,%l6,%l6
	 xor	 %l2,%l3,%l4
	add	%l6,%l1,%l1
	 srlx	%o2,32,%g3		! extract X[5]
	and	%l1,%l4,%l4		! round 4
	add	%l5,%l0,%l0
	xor	%l3,%l4,%l4
	 sethi	%hi(1200080426),%l5
	add	%l4,%l0,%l0
	 or	%l5,%lo(1200080426),%l5
	sll	%l0,7,%l6
	 add	%g3,%l5,%l5		! X[5]+K[5]
	srl	%l0,32-7,%l0
	add	%l1,%l6,%l6
	 xor	 %l1,%l2,%l4
	add	%l6,%l0,%l0
	 srlx	%o3,%i3,%o3	! align X[6]
	and	%l0,%l4,%l4		! round 5
	 sllx	%o4,%i4,%g3
	add	%l5,%l3,%l3
	 sllx	%g3,%i5,%g3
	xor	%l2,%l4,%l4
	 or	%g3,%o3,%o3
	 sethi	%hi(2821735955),%l5
	add	%l4,%l3,%l3
	 or	%l5,%lo(2821735955),%l5
	sll	%l3,12,%l6
	 add	%o3,%l5,%l5		! X[6]+K[6]
	srl	%l3,32-12,%l3
	add	%l0,%l6,%l6
	 xor	 %l0,%l1,%l4
	add	%l6,%l3,%l3
	 srlx	%o3,32,%g3		! extract X[7]
	and	%l3,%l4,%l4		! round 6
	add	%l5,%l2,%l2
	xor	%l1,%l4,%l4
	 sethi	%hi(4249261313),%l5
	add	%l4,%l2,%l2
	 or	%l5,%lo(4249261313),%l5
	sll	%l2,17,%l6
	 add	%g3,%l5,%l5		! X[7]+K[7]
	srl	%l2,32-17,%l2
	add	%l3,%l6,%l6
	 xor	 %l3,%l0,%l4
	add	%l6,%l2,%l2
	 srlx	%o4,%i3,%o4	! align X[8]
	and	%l2,%l4,%l4		! round 7
	 sllx	%o5,%i4,%g3
	add	%l5,%l1,%l1
	 sllx	%g3,%i5,%g3
	xor	%l0,%l4,%l4
	 or	%g3,%o4,%o4
	 sethi	%hi(1770035416),%l5
	add	%l4,%l1,%l1
	 or	%l5,%lo(1770035416),%l5
	sll	%l1,22,%l6
	 add	%o4,%l5,%l5		! X[8]+K[8]
	srl	%l1,32-22,%l1
	add	%l2,%l6,%l6
	 xor	 %l2,%l3,%l4
	add	%l6,%l1,%l1
	 srlx	%o4,32,%g3		! extract X[9]
	and	%l1,%l4,%l4		! round 8
	add	%l5,%l0,%l0
	xor	%l3,%l4,%l4
	 sethi	%hi(2336552879),%l5
	add	%l4,%l0,%l0
	 or	%l5,%lo(2336552879),%l5
	sll	%l0,7,%l6
	 add	%g3,%l5,%l5		! X[9]+K[9]
	srl	%l0,32-7,%l0
	add	%l1,%l6,%l6
	 xor	 %l1,%l2,%l4
	add	%l6,%l0,%l0
	 srlx	%o5,%i3,%o5	! align X[10]
	and	%l0,%l4,%l4		! round 9
	 sllx	%o7,%i4,%g3
	add	%l5,%l3,%l3
	 sllx	%g3,%i5,%g3
	xor	%l2,%l4,%l4
	 or	%g3,%o5,%o5
	 sethi	%hi(4294925233),%l5
	add	%l4,%l3,%l3
	 or	%l5,%lo(4294925233),%l5
	sll	%l3,12,%l6
	 add	%o5,%l5,%l5		! X[10]+K[10]
	srl	%l3,32-12,%l3
	add	%l0,%l6,%l6
	 xor	 %l0,%l1,%l4
	add	%l6,%l3,%l3
	 srlx	%o5,32,%g3		! extract X[11]
	and	%l3,%l4,%l4		! round 10
	add	%l5,%l2,%l2
	xor	%l1,%l4,%l4
	 sethi	%hi(2304563134),%l5
	add	%l4,%l2,%l2
	 or	%l5,%lo(2304563134),%l5
	sll	%l2,17,%l6
	 add	%g3,%l5,%l5		! X[11]+K[11]
	srl	%l2,32-17,%l2
	add	%l3,%l6,%l6
	 xor	 %l3,%l0,%l4
	add	%l6,%l2,%l2
	 srlx	%o7,%i3,%o7	! align X[12]
	and	%l2,%l4,%l4		! round 11
	 sllx	%g1,%i4,%g3
	add	%l5,%l1,%l1
	 sllx	%g3,%i5,%g3
	xor	%l0,%l4,%l4
	 or	%g3,%o7,%o7
	 sethi	%hi(1804603682),%l5
	add	%l4,%l1,%l1
	 or	%l5,%lo(1804603682),%l5
	sll	%l1,22,%l6
	 add	%o7,%l5,%l5		! X[12]+K[12]
	srl	%l1,32-22,%l1
	add	%l2,%l6,%l6
	 xor	 %l2,%l3,%l4
	add	%l6,%l1,%l1
	 srlx	%o7,32,%g3		! extract X[13]
	and	%l1,%l4,%l4		! round 12
	add	%l5,%l0,%l0
	xor	%l3,%l4,%l4
	 sethi	%hi(4254626195),%l5
	add	%l4,%l0,%l0
	 or	%l5,%lo(4254626195),%l5
	sll	%l0,7,%l6
	 add	%g3,%l5,%l5		! X[13]+K[13]
	srl	%l0,32-7,%l0
	add	%l1,%l6,%l6
	 xor	 %l1,%l2,%l4
	add	%l6,%l0,%l0
	 srlx	%g1,%i3,%g1	! align X[14]
	and	%l0,%l4,%l4		! round 13
	 sllx	%g2,%i4,%g3
	add	%l5,%l3,%l3
	 sllx	%g3,%i5,%g3
	xor	%l2,%l4,%l4
	 or	%g3,%g1,%g1
	 sethi	%hi(2792965006),%l5
	add	%l4,%l3,%l3
	 or	%l5,%lo(2792965006),%l5
	sll	%l3,12,%l6
	 add	%g1,%l5,%l5		! X[14]+K[14]
	srl	%l3,32-12,%l3
	add	%l0,%l6,%l6
	 xor	 %l0,%l1,%l4
	add	%l6,%l3,%l3
	 srlx	%g1,32,%g3		! extract X[15]
	and	%l3,%l4,%l4		! round 14
	add	%l5,%l2,%l2
	xor	%l1,%l4,%l4
	 sethi	%hi(1236535329),%l5
	add	%l4,%l2,%l2
	 or	%l5,%lo(1236535329),%l5
	sll	%l2,17,%l6
	 add	%g3,%l5,%l5		! X[15]+K[15]
	srl	%l2,32-17,%l2
	add	%l3,%l6,%l6
	 xor	 %l3,%l0,%l4
	add	%l6,%l2,%l2
	 srlx	%o0,32,%g3		! extract X[1]
	and	%l2,%l4,%l4		! round 15
	add	%l5,%l1,%l1
	xor	%l0,%l4,%l4
	 sethi	%hi(4129170786),%l5
	add	%l4,%l1,%l1
	 or	%l5,%lo(4129170786),%l5
	sll	%l1,22,%l6
	 add	%g3,%l5,%l5		! X[1]+K[16]
	srl	%l1,32-22,%l1
	add	%l2,%l6,%l6
	 andn	 %l2,%l3,%l4
	add	%l6,%l1,%l1
	and	%l1,%l3,%l6		! round 16
	add	%l5,%l0,%l0
	or	%l6,%l4,%l4
	 sethi	%hi(3225465664),%l5
	add	%l4,%l0,%l0
	 or	%l5,%lo(3225465664),%l5
	sll	%l0,5,%l6
	 add	%o3,%l5,%l5		! X[6]+K[17]
	srl	%l0,32-5,%l0
	add	%l1,%l6,%l6
	 andn	 %l1,%l2,%l4
	add	%l6,%l0,%l0
	 srlx	%o5,32,%g3		! extract X[11]
	and	%l0,%l2,%l6		! round 17
	add	%l5,%l3,%l3
	or	%l6,%l4,%l4
	 sethi	%hi(643717713),%l5
	add	%l4,%l3,%l3
	 or	%l5,%lo(643717713),%l5
	sll	%l3,9,%l6
	 add	%g3,%l5,%l5		! X[11]+K[18]
	srl	%l3,32-9,%l3
	add	%l0,%l6,%l6
	 andn	 %l0,%l1,%l4
	add	%l6,%l3,%l3
	and	%l3,%l1,%l6		! round 18
	add	%l5,%l2,%l2
	or	%l6,%l4,%l4
	 sethi	%hi(3921069994),%l5
	add	%l4,%l2,%l2
	 or	%l5,%lo(3921069994),%l5
	sll	%l2,14,%l6
	 add	%o0,%l5,%l5		! X[0]+K[19]
	srl	%l2,32-14,%l2
	add	%l3,%l6,%l6
	 andn	 %l3,%l0,%l4
	add	%l6,%l2,%l2
	 srlx	%o2,32,%g3		! extract X[5]
	and	%l2,%l0,%l6		! round 19
	add	%l5,%l1,%l1
	or	%l6,%l4,%l4
	 sethi	%hi(3593408605),%l5
	add	%l4,%l1,%l1
	 or	%l5,%lo(3593408605),%l5
	sll	%l1,20,%l6
	 add	%g3,%l5,%l5		! X[5]+K[20]
	srl	%l1,32-20,%l1
	add	%l2,%l6,%l6
	 andn	 %l2,%l3,%l4
	add	%l6,%l1,%l1
	and	%l1,%l3,%l6		! round 20
	add	%l5,%l0,%l0
	or	%l6,%l4,%l4
	 sethi	%hi(38016083),%l5
	add	%l4,%l0,%l0
	 or	%l5,%lo(38016083),%l5
	sll	%l0,5,%l6
	 add	%o5,%l5,%l5		! X[10]+K[21]
	srl	%l0,32-5,%l0
	add	%l1,%l6,%l6
	 andn	 %l1,%l2,%l4
	add	%l6,%l0,%l0
	 srlx	%g1,32,%g3		! extract X[15]
	and	%l0,%l2,%l6		! round 21
	add	%l5,%l3,%l3
	or	%l6,%l4,%l4
	 sethi	%hi(3634488961),%l5
	add	%l4,%l3,%l3
	 or	%l5,%lo(3634488961),%l5
	sll	%l3,9,%l6
	 add	%g3,%l5,%l5		! X[15]+K[22]
	srl	%l3,32-9,%l3
	add	%l0,%l6,%l6
	 andn	 %l0,%l1,%l4
	add	%l6,%l3,%l3
	and	%l3,%l1,%l6		! round 22
	add	%l5,%l2,%l2
	or	%l6,%l4,%l4
	 sethi	%hi(3889429448),%l5
	add	%l4,%l2,%l2
	 or	%l5,%lo(3889429448),%l5
	sll	%l2,14,%l6
	 add	%o2,%l5,%l5		! X[4]+K[23]
	srl	%l2,32-14,%l2
	add	%l3,%l6,%l6
	 andn	 %l3,%l0,%l4
	add	%l6,%l2,%l2
	 srlx	%o4,32,%g3		! extract X[9]
	and	%l2,%l0,%l6		! round 23
	add	%l5,%l1,%l1
	or	%l6,%l4,%l4
	 sethi	%hi(568446438),%l5
	add	%l4,%l1,%l1
	 or	%l5,%lo(568446438),%l5
	sll	%l1,20,%l6
	 add	%g3,%l5,%l5		! X[9]+K[24]
	srl	%l1,32-20,%l1
	add	%l2,%l6,%l6
	 andn	 %l2,%l3,%l4
	add	%l6,%l1,%l1
	and	%l1,%l3,%l6		! round 24
	add	%l5,%l0,%l0
	or	%l6,%l4,%l4
	 sethi	%hi(3275163606),%l5
	add	%l4,%l0,%l0
	 or	%l5,%lo(3275163606),%l5
	sll	%l0,5,%l6
	 add	%g1,%l5,%l5		! X[14]+K[25]
	srl	%l0,32-5,%l0
	add	%l1,%l6,%l6
	 andn	 %l1,%l2,%l4
	add	%l6,%l0,%l0
	 srlx	%o1,32,%g3		! extract X[3]
	and	%l0,%l2,%l6		! round 25
	add	%l5,%l3,%l3
	or	%l6,%l4,%l4
	 sethi	%hi(4107603335),%l5
	add	%l4,%l3,%l3
	 or	%l5,%lo(4107603335),%l5
	sll	%l3,9,%l6
	 add	%g3,%l5,%l5		! X[3]+K[26]
	srl	%l3,32-9,%l3
	add	%l0,%l6,%l6
	 andn	 %l0,%l1,%l4
	add	%l6,%l3,%l3
	and	%l3,%l1,%l6		! round 26
	add	%l5,%l2,%l2
	or	%l6,%l4,%l4
	 sethi	%hi(1163531501),%l5
	add	%l4,%l2,%l2
	 or	%l5,%lo(1163531501),%l5
	sll	%l2,14,%l6
	 add	%o4,%l5,%l5		! X[8]+K[27]
	srl	%l2,32-14,%l2
	add	%l3,%l6,%l6
	 andn	 %l3,%l0,%l4
	add	%l6,%l2,%l2
	 srlx	%o7,32,%g3		! extract X[13]
	and	%l2,%l0,%l6		! round 27
	add	%l5,%l1,%l1
	or	%l6,%l4,%l4
	 sethi	%hi(2850285829),%l5
	add	%l4,%l1,%l1
	 or	%l5,%lo(2850285829),%l5
	sll	%l1,20,%l6
	 add	%g3,%l5,%l5		! X[13]+K[28]
	srl	%l1,32-20,%l1
	add	%l2,%l6,%l6
	 andn	 %l2,%l3,%l4
	add	%l6,%l1,%l1
	and	%l1,%l3,%l6		! round 28
	add	%l5,%l0,%l0
	or	%l6,%l4,%l4
	 sethi	%hi(4243563512),%l5
	add	%l4,%l0,%l0
	 or	%l5,%lo(4243563512),%l5
	sll	%l0,5,%l6
	 add	%o1,%l5,%l5		! X[2]+K[29]
	srl	%l0,32-5,%l0
	add	%l1,%l6,%l6
	 andn	 %l1,%l2,%l4
	add	%l6,%l0,%l0
	 srlx	%o3,32,%g3		! extract X[7]
	and	%l0,%l2,%l6		! round 29
	add	%l5,%l3,%l3
	or	%l6,%l4,%l4
	 sethi	%hi(1735328473),%l5
	add	%l4,%l3,%l3
	 or	%l5,%lo(1735328473),%l5
	sll	%l3,9,%l6
	 add	%g3,%l5,%l5		! X[7]+K[30]
	srl	%l3,32-9,%l3
	add	%l0,%l6,%l6
	 andn	 %l0,%l1,%l4
	add	%l6,%l3,%l3
	and	%l3,%l1,%l6		! round 30
	add	%l5,%l2,%l2
	or	%l6,%l4,%l4
	 sethi	%hi(2368359562),%l5
	add	%l4,%l2,%l2
	 or	%l5,%lo(2368359562),%l5
	sll	%l2,14,%l6
	 add	%o7,%l5,%l5		! X[12]+K[31]
	srl	%l2,32-14,%l2
	add	%l3,%l6,%l6
	 andn	 %l3,%l0,%l4
	add	%l6,%l2,%l2
	 srlx	%o2,32,%g3		! extract X[5]
	and	%l2,%l0,%l6		! round 31
	add	%l5,%l1,%l1
	or	%l6,%l4,%l4
	 sethi	%hi(4294588738),%l5
	add	%l4,%l1,%l1
	 or	%l5,%lo(4294588738),%l5
	sll	%l1,20,%l6
	 add	%g3,%l5,%l5		! X[5]+K[32]
	srl	%l1,32-20,%l1
	add	%l2,%l6,%l6
	 xor	 %l2,%l3,%l4
	add	%l6,%l1,%l1
	add	%l5,%l0,%l0		! round 32
	xor	%l1,%l4,%l4
	 sethi	%hi(2272392833),%l5
	add	%l4,%l0,%l0
	 or	%l5,%lo(2272392833),%l5
	sll	%l0,4,%l6
	 add	%o4,%l5,%l5		! X[8]+K[33]
	srl	%l0,32-4,%l0
	add	%l1,%l6,%l6
	 xor	 %l1,%l2,%l4
	add	%l6,%l0,%l0
	 srlx	%o5,32,%g3		! extract X[11]
	add	%l5,%l3,%l3		! round 33
	xor	%l0,%l4,%l4
	 sethi	%hi(1839030562),%l5
	add	%l4,%l3,%l3
	 or	%l5,%lo(1839030562),%l5
	sll	%l3,11,%l6
	 add	%g3,%l5,%l5		! X[11]+K[34]
	srl	%l3,32-11,%l3
	add	%l0,%l6,%l6
	 xor	 %l0,%l1,%l4
	add	%l6,%l3,%l3
	add	%l5,%l2,%l2		! round 34
	xor	%l3,%l4,%l4
	 sethi	%hi(4259657740),%l5
	add	%l4,%l2,%l2
	 or	%l5,%lo(4259657740),%l5
	sll	%l2,16,%l6
	 add	%g1,%l5,%l5		! X[14]+K[35]
	srl	%l2,32-16,%l2
	add	%l3,%l6,%l6
	 xor	 %l3,%l0,%l4
	add	%l6,%l2,%l2
	 srlx	%o0,32,%g3		! extract X[1]
	add	%l5,%l1,%l1		! round 35
	xor	%l2,%l4,%l4
	 sethi	%hi(2763975236),%l5
	add	%l4,%l1,%l1
	 or	%l5,%lo(2763975236),%l5
	sll	%l1,23,%l6
	 add	%g3,%l5,%l5		! X[1]+K[36]
	srl	%l1,32-23,%l1
	add	%l2,%l6,%l6
	 xor	 %l2,%l3,%l4
	add	%l6,%l1,%l1
	add	%l5,%l0,%l0		! round 36
	xor	%l1,%l4,%l4
	 sethi	%hi(1272893353),%l5
	add	%l4,%l0,%l0
	 or	%l5,%lo(1272893353),%l5
	sll	%l0,4,%l6
	 add	%o2,%l5,%l5		! X[4]+K[37]
	srl	%l0,32-4,%l0
	add	%l1,%l6,%l6
	 xor	 %l1,%l2,%l4
	add	%l6,%l0,%l0
	 srlx	%o3,32,%g3		! extract X[7]
	add	%l5,%l3,%l3		! round 37
	xor	%l0,%l4,%l4
	 sethi	%hi(4139469664),%l5
	add	%l4,%l3,%l3
	 or	%l5,%lo(4139469664),%l5
	sll	%l3,11,%l6
	 add	%g3,%l5,%l5		! X[7]+K[38]
	srl	%l3,32-11,%l3
	add	%l0,%l6,%l6
	 xor	 %l0,%l1,%l4
	add	%l6,%l3,%l3
	add	%l5,%l2,%l2		! round 38
	xor	%l3,%l4,%l4
	 sethi	%hi(3200236656),%l5
	add	%l4,%l2,%l2
	 or	%l5,%lo(3200236656),%l5
	sll	%l2,16,%l6
	 add	%o5,%l5,%l5		! X[10]+K[39]
	srl	%l2,32-16,%l2
	add	%l3,%l6,%l6
	 xor	 %l3,%l0,%l4
	add	%l6,%l2,%l2
	 srlx	%o7,32,%g3		! extract X[13]
	add	%l5,%l1,%l1		! round 39
	xor	%l2,%l4,%l4
	 sethi	%hi(681279174),%l5
	add	%l4,%l1,%l1
	 or	%l5,%lo(681279174),%l5
	sll	%l1,23,%l6
	 add	%g3,%l5,%l5		! X[13]+K[40]
	srl	%l1,32-23,%l1
	add	%l2,%l6,%l6
	 xor	 %l2,%l3,%l4
	add	%l6,%l1,%l1
	add	%l5,%l0,%l0		! round 40
	xor	%l1,%l4,%l4
	 sethi	%hi(3936430074),%l5
	add	%l4,%l0,%l0
	 or	%l5,%lo(3936430074),%l5
	sll	%l0,4,%l6
	 add	%o0,%l5,%l5		! X[0]+K[41]
	srl	%l0,32-4,%l0
	add	%l1,%l6,%l6
	 xor	 %l1,%l2,%l4
	add	%l6,%l0,%l0
	 srlx	%o1,32,%g3		! extract X[3]
	add	%l5,%l3,%l3		! round 41
	xor	%l0,%l4,%l4
	 sethi	%hi(3572445317),%l5
	add	%l4,%l3,%l3
	 or	%l5,%lo(3572445317),%l5
	sll	%l3,11,%l6
	 add	%g3,%l5,%l5		! X[3]+K[42]
	srl	%l3,32-11,%l3
	add	%l0,%l6,%l6
	 xor	 %l0,%l1,%l4
	add	%l6,%l3,%l3
	add	%l5,%l2,%l2		! round 42
	xor	%l3,%l4,%l4
	 sethi	%hi(76029189),%l5
	add	%l4,%l2,%l2
	 or	%l5,%lo(76029189),%l5
	sll	%l2,16,%l6
	 add	%o3,%l5,%l5		! X[6]+K[43]
	srl	%l2,32-16,%l2
	add	%l3,%l6,%l6
	 xor	 %l3,%l0,%l4
	add	%l6,%l2,%l2
	 srlx	%o4,32,%g3		! extract X[9]
	add	%l5,%l1,%l1		! round 43
	xor	%l2,%l4,%l4
	 sethi	%hi(3654602809),%l5
	add	%l4,%l1,%l1
	 or	%l5,%lo(3654602809),%l5
	sll	%l1,23,%l6
	 add	%g3,%l5,%l5		! X[9]+K[44]
	srl	%l1,32-23,%l1
	add	%l2,%l6,%l6
	 xor	 %l2,%l3,%l4
	add	%l6,%l1,%l1
	add	%l5,%l0,%l0		! round 44
	xor	%l1,%l4,%l4
	 sethi	%hi(3873151461),%l5
	add	%l4,%l0,%l0
	 or	%l5,%lo(3873151461),%l5
	sll	%l0,4,%l6
	 add	%o7,%l5,%l5		! X[12]+K[45]
	srl	%l0,32-4,%l0
	add	%l1,%l6,%l6
	 xor	 %l1,%l2,%l4
	add	%l6,%l0,%l0
	 srlx	%g1,32,%g3		! extract X[15]
	add	%l5,%l3,%l3		! round 45
	xor	%l0,%l4,%l4
	 sethi	%hi(530742520),%l5
	add	%l4,%l3,%l3
	 or	%l5,%lo(530742520),%l5
	sll	%l3,11,%l6
	 add	%g3,%l5,%l5		! X[15]+K[46]
	srl	%l3,32-11,%l3
	add	%l0,%l6,%l6
	 xor	 %l0,%l1,%l4
	add	%l6,%l3,%l3
	add	%l5,%l2,%l2		! round 46
	xor	%l3,%l4,%l4
	 sethi	%hi(3299628645),%l5
	add	%l4,%l2,%l2
	 or	%l5,%lo(3299628645),%l5
	sll	%l2,16,%l6
	 add	%o1,%l5,%l5		! X[2]+K[47]
	srl	%l2,32-16,%l2
	add	%l3,%l6,%l6
	 xor	 %l3,%l0,%l4
	add	%l6,%l2,%l2
	add	%l5,%l1,%l1		! round 47
	xor	%l2,%l4,%l4
	 sethi	%hi(4096336452),%l5
	add	%l4,%l1,%l1
	 or	%l5,%lo(4096336452),%l5
	sll	%l1,23,%l6
	 add	%o0,%l5,%l5		! X[0]+K[48]
	srl	%l1,32-23,%l1
	add	%l2,%l6,%l6
	 xor	 %l2,%l3,%l4
	add	%l6,%l1,%l1
	add	%l5,%l0,%l0		! round 48
	 srlx	%o3,32,%g3		! extract X[7]
	orn	%l1,%l3,%l4
	 sethi	%hi(1126891415),%l5
	xor	%l2,%l4,%l4
	 or	%l5,%lo(1126891415),%l5
	add	%l4,%l0,%l0
	sll	%l0,6,%l6
	 add	%g3,%l5,%l5		! X[7]+K[49]
	srl	%l0,32-6,%l0
	add	%l1,%l6,%l6
	add	%l6,%l0,%l0
	add	%l5,%l3,%l3		! round 49
	orn	%l0,%l2,%l4
	 sethi	%hi(2878612391),%l5
	xor	%l1,%l4,%l4
	 or	%l5,%lo(2878612391),%l5
	add	%l4,%l3,%l3
	sll	%l3,10,%l6
	 add	%g1,%l5,%l5		! X[14]+K[50]
	srl	%l3,32-10,%l3
	add	%l0,%l6,%l6
	add	%l6,%l3,%l3
	add	%l5,%l2,%l2		! round 50
	 srlx	%o2,32,%g3		! extract X[5]
	orn	%l3,%l1,%l4
	 sethi	%hi(4237533241),%l5
	xor	%l0,%l4,%l4
	 or	%l5,%lo(4237533241),%l5
	add	%l4,%l2,%l2
	sll	%l2,15,%l6
	 add	%g3,%l5,%l5		! X[5]+K[51]
	srl	%l2,32-15,%l2
	add	%l3,%l6,%l6
	add	%l6,%l2,%l2
	add	%l5,%l1,%l1		! round 51
	orn	%l2,%l0,%l4
	 sethi	%hi(1700485571),%l5
	xor	%l3,%l4,%l4
	 or	%l5,%lo(1700485571),%l5
	add	%l4,%l1,%l1
	sll	%l1,21,%l6
	 add	%o7,%l5,%l5		! X[12]+K[52]
	srl	%l1,32-21,%l1
	add	%l2,%l6,%l6
	add	%l6,%l1,%l1
	add	%l5,%l0,%l0		! round 52
	 srlx	%o1,32,%g3		! extract X[3]
	orn	%l1,%l3,%l4
	 sethi	%hi(2399980690),%l5
	xor	%l2,%l4,%l4
	 or	%l5,%lo(2399980690),%l5
	add	%l4,%l0,%l0
	sll	%l0,6,%l6
	 add	%g3,%l5,%l5		! X[3]+K[53]
	srl	%l0,32-6,%l0
	add	%l1,%l6,%l6
	add	%l6,%l0,%l0
	add	%l5,%l3,%l3		! round 53
	orn	%l0,%l2,%l4
	 sethi	%hi(4293915773),%l5
	xor	%l1,%l4,%l4
	 or	%l5,%lo(4293915773),%l5
	add	%l4,%l3,%l3
	sll	%l3,10,%l6
	 add	%o5,%l5,%l5		! X[10]+K[54]
	srl	%l3,32-10,%l3
	add	%l0,%l6,%l6
	add	%l6,%l3,%l3
	add	%l5,%l2,%l2		! round 54
	 srlx	%o0,32,%g3		! extract X[1]
	orn	%l3,%l1,%l4
	 sethi	%hi(2240044497),%l5
	xor	%l0,%l4,%l4
	 or	%l5,%lo(2240044497),%l5
	add	%l4,%l2,%l2
	sll	%l2,15,%l6
	 add	%g3,%l5,%l5		! X[1]+K[55]
	srl	%l2,32-15,%l2
	add	%l3,%l6,%l6
	add	%l6,%l2,%l2
	add	%l5,%l1,%l1		! round 55
	orn	%l2,%l0,%l4
	 sethi	%hi(1873313359),%l5
	xor	%l3,%l4,%l4
	 or	%l5,%lo(1873313359),%l5
	add	%l4,%l1,%l1
	sll	%l1,21,%l6
	 add	%o4,%l5,%l5		! X[8]+K[56]
	srl	%l1,32-21,%l1
	add	%l2,%l6,%l6
	add	%l6,%l1,%l1
	add	%l5,%l0,%l0		! round 56
	 srlx	%g1,32,%g3		! extract X[15]
	orn	%l1,%l3,%l4
	 sethi	%hi(4264355552),%l5
	xor	%l2,%l4,%l4
	 or	%l5,%lo(4264355552),%l5
	add	%l4,%l0,%l0
	sll	%l0,6,%l6
	 add	%g3,%l5,%l5		! X[15]+K[57]
	srl	%l0,32-6,%l0
	add	%l1,%l6,%l6
	add	%l6,%l0,%l0
	add	%l5,%l3,%l3		! round 57
	orn	%l0,%l2,%l4
	 sethi	%hi(2734768916),%l5
	xor	%l1,%l4,%l4
	 or	%l5,%lo(2734768916),%l5
	add	%l4,%l3,%l3
	sll	%l3,10,%l6
	 add	%o3,%l5,%l5		! X[6]+K[58]
	srl	%l3,32-10,%l3
	add	%l0,%l6,%l6
	add	%l6,%l3,%l3
	add	%l5,%l2,%l2		! round 58
	 srlx	%o7,32,%g3		! extract X[13]
	orn	%l3,%l1,%l4
	 sethi	%hi(1309151649),%l5
	xor	%l0,%l4,%l4
	 or	%l5,%lo(1309151649),%l5
	add	%l4,%l2,%l2
	sll	%l2,15,%l6
	 add	%g3,%l5,%l5		! X[13]+K[59]
	srl	%l2,32-15,%l2
	add	%l3,%l6,%l6
	add	%l6,%l2,%l2
	add	%l5,%l1,%l1		! round 59
	orn	%l2,%l0,%l4
	 sethi	%hi(4149444226),%l5
	xor	%l3,%l4,%l4
	 or	%l5,%lo(4149444226),%l5
	add	%l4,%l1,%l1
	sll	%l1,21,%l6
	 add	%o2,%l5,%l5		! X[4]+K[60]
	srl	%l1,32-21,%l1
	add	%l2,%l6,%l6
	add	%l6,%l1,%l1
	add	%l5,%l0,%l0		! round 60
	 srlx	%o5,32,%g3		! extract X[11]
	orn	%l1,%l3,%l4
	 sethi	%hi(3174756917),%l5
	xor	%l2,%l4,%l4
	 or	%l5,%lo(3174756917),%l5
	add	%l4,%l0,%l0
	sll	%l0,6,%l6
	 add	%g3,%l5,%l5		! X[11]+K[61]
	srl	%l0,32-6,%l0
	add	%l1,%l6,%l6
	add	%l6,%l0,%l0
	add	%l5,%l3,%l3		! round 61
	orn	%l0,%l2,%l4
	 sethi	%hi(718787259),%l5
	xor	%l1,%l4,%l4
	 or	%l5,%lo(718787259),%l5
	add	%l4,%l3,%l3
	sll	%l3,10,%l6
	 add	%o1,%l5,%l5		! X[2]+K[62]
	srl	%l3,32-10,%l3
	add	%l0,%l6,%l6
	add	%l6,%l3,%l3
	add	%l5,%l2,%l2		! round 62
	 srlx	%o4,32,%g3		! extract X[9]
	orn	%l3,%l1,%l4
	 sethi	%hi(3951481745),%l5
	xor	%l0,%l4,%l4
	 or	%l5,%lo(3951481745),%l5
	add	%l4,%l2,%l2
	sll	%l2,15,%l6
	 add	%g3,%l5,%l5		! X[9]+K[63]
	srl	%l2,32-15,%l2
	add	%l3,%l6,%l6
	add	%l6,%l2,%l2
	add	%l5,%l1,%l1		! round 63
	orn	%l2,%l0,%l4
	 sethi	%hi(0),%l5
	xor	%l3,%l4,%l4
	 or	%l5,%lo(0),%l5
	add	%l4,%l1,%l1
	sll	%l1,21,%l6
	 add	%o0,%l5,%l5		! X[0]+K[64]
	srl	%l1,32-21,%l1
	add	%l2,%l6,%l6
	add	%l6,%l1,%l1
	srlx	%g4,32,%l4		! unpack A,B,C,D and accumulate
	add	%i1,64,%i1		! advance inp
	srlx	%g5,32,%l5
	add	%l4,%l0,%l0
	subcc	%i2,1,%i2		! done yet?
	add	%g4,%l1,%l1
	add	%l5,%l2,%l2
	add	%g5,%l3,%l3
	srl	%l1,0,%l1			! clruw	%l1
	bne	SIZE_T_CC,.Loop
	srl	%l3,0,%l3			! clruw	%l3

	st	%l0,[%i0+0]		! write out ctx
	st	%l1,[%i0+4]
	st	%l2,[%i0+8]
	st	%l3,[%i0+12]

	wr	%g0,%l7,%asi
	ret
	restore
.type	md5_block_asm_data_order,#function
.size	md5_block_asm_data_order,(.-md5_block_asm_data_order)

.asciz	"MD5 block transform for SPARCv9, CRYPTOGAMS by <appro@openssl.org>"
.align	4
