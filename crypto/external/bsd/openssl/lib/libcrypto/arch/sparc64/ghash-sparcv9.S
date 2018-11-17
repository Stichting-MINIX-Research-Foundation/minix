.register	%g2,#scratch
.register	%g3,#scratch
.section	".text",#alloc,#execinstr

.align	64
rem_4bit:
	.long	0,0,471859200,0,943718400,0,610271232,0
	.long	1887436800,0,1822425088,0,1220542464,0,1423966208,0
	.long	3774873600,0,4246732800,0,3644850176,0,3311403008,0
	.long	2441084928,0,2376073216,0,2847932416,0,3051356160,0
.type	rem_4bit,#object
.size	rem_4bit,(.-rem_4bit)

.globl	gcm_ghash_4bit
.align	32
gcm_ghash_4bit:
	save	%sp,-192,%sp
	ldub	[%i2+15],%l1
	ldub	[%i0+15],%l2
	ldub	[%i0+14],%l3
	add	%i3,%i2,%i3
	add	%i1,8,%l6

1:	call	.+8
	add	%o7,rem_4bit-1b,%l4

.Louter:
	xor	%l2,%l1,%l1
	and	%l1,0xf0,%l0
	and	%l1,0x0f,%l1
	sll	%l1,4,%l1
	ldx	[%l6+%l1],%o1
	ldx	[%i1+%l1],%o0

	ldub	[%i2+14],%l1

	ldx	[%l6+%l0],%o3
	and	%o1,0xf,%l5
	ldx	[%i1+%l0],%o2
	sll	%l5,3,%l5
	ldx	[%l4+%l5],%o4
	srlx	%o1,4,%o1
	mov	13,%l7
	sllx	%o0,60,%o5
	xor	%o3,%o1,%o1
	srlx	%o0,4,%o0
	xor	%o1,%o5,%o1

	xor	%l3,%l1,%l1
	and	%o1,0xf,%l5
	and	%l1,0xf0,%l0
	and	%l1,0x0f,%l1
	ba	.Lghash_inner
	sll	%l1,4,%l1
.align	32
.Lghash_inner:
	ldx	[%l6+%l1],%o3
	sll	%l5,3,%l5
	xor	%o2,%o0,%o0
	ldx	[%i1+%l1],%o2
	srlx	%o1,4,%o1
	xor	%o4,%o0,%o0
	ldx	[%l4+%l5],%o4
	sllx	%o0,60,%o5
	xor	%o3,%o1,%o1
	ldub	[%i2+%l7],%l1
	srlx	%o0,4,%o0
	xor	%o1,%o5,%o1
	ldub	[%i0+%l7],%l3
	xor	%o2,%o0,%o0
	and	%o1,0xf,%l5

	ldx	[%l6+%l0],%o3
	sll	%l5,3,%l5
	xor	%o4,%o0,%o0
	ldx	[%i1+%l0],%o2
	srlx	%o1,4,%o1
	ldx	[%l4+%l5],%o4
	sllx	%o0,60,%o5
	xor	%l3,%l1,%l1
	srlx	%o0,4,%o0
	and	%l1,0xf0,%l0
	addcc	%l7,-1,%l7
	xor	%o1,%o5,%o1
	and	%l1,0x0f,%l1
	xor	%o3,%o1,%o1
	sll	%l1,4,%l1
	blu	.Lghash_inner
	and	%o1,0xf,%l5

	ldx	[%l6+%l1],%o3
	sll	%l5,3,%l5
	xor	%o2,%o0,%o0
	ldx	[%i1+%l1],%o2
	srlx	%o1,4,%o1
	xor	%o4,%o0,%o0
	ldx	[%l4+%l5],%o4
	sllx	%o0,60,%o5
	xor	%o3,%o1,%o1
	srlx	%o0,4,%o0
	xor	%o1,%o5,%o1
	xor	%o2,%o0,%o0

	add	%i2,16,%i2
	cmp	%i2,%i3
	be,pn	%xcc,.Ldone
	and	%o1,0xf,%l5

	ldx	[%l6+%l0],%o3
	sll	%l5,3,%l5
	xor	%o4,%o0,%o0
	ldx	[%i1+%l0],%o2
	srlx	%o1,4,%o1
	ldx	[%l4+%l5],%o4
	sllx	%o0,60,%o5
	xor	%o3,%o1,%o1
	ldub	[%i2+15],%l1
	srlx	%o0,4,%o0
	xor	%o1,%o5,%o1
	xor	%o2,%o0,%o0
	stx	%o1,[%i0+8]
	xor	%o4,%o0,%o0
	stx	%o0,[%i0]
	srl	%o1,8,%l3
	and	%o1,0xff,%l2
	ba	.Louter
	and	%l3,0xff,%l3
.align	32
.Ldone:
	ldx	[%l6+%l0],%o3
	sll	%l5,3,%l5
	xor	%o4,%o0,%o0
	ldx	[%i1+%l0],%o2
	srlx	%o1,4,%o1
	ldx	[%l4+%l5],%o4
	sllx	%o0,60,%o5
	xor	%o3,%o1,%o1
	srlx	%o0,4,%o0
	xor	%o1,%o5,%o1
	xor	%o2,%o0,%o0
	stx	%o1,[%i0+8]
	xor	%o4,%o0,%o0
	stx	%o0,[%i0]

	ret
	restore
.type	gcm_ghash_4bit,#function
.size	gcm_ghash_4bit,(.-gcm_ghash_4bit)
.globl	gcm_gmult_4bit
.align	32
gcm_gmult_4bit:
	save	%sp,-192,%sp
	ldub	[%i0+15],%l1
	add	%i1,8,%l6

1:	call	.+8
	add	%o7,rem_4bit-1b,%l4

	and	%l1,0xf0,%l0
	and	%l1,0x0f,%l1
	sll	%l1,4,%l1
	ldx	[%l6+%l1],%o1
	ldx	[%i1+%l1],%o0

	ldub	[%i0+14],%l1

	ldx	[%l6+%l0],%o3
	and	%o1,0xf,%l5
	ldx	[%i1+%l0],%o2
	sll	%l5,3,%l5
	ldx	[%l4+%l5],%o4
	srlx	%o1,4,%o1
	mov	13,%l7
	sllx	%o0,60,%o5
	xor	%o3,%o1,%o1
	srlx	%o0,4,%o0
	xor	%o1,%o5,%o1

	and	%o1,0xf,%l5
	and	%l1,0xf0,%l0
	and	%l1,0x0f,%l1
	ba	.Lgmult_inner
	sll	%l1,4,%l1
.align	32
.Lgmult_inner:
	ldx	[%l6+%l1],%o3
	sll	%l5,3,%l5
	xor	%o2,%o0,%o0
	ldx	[%i1+%l1],%o2
	srlx	%o1,4,%o1
	xor	%o4,%o0,%o0
	ldx	[%l4+%l5],%o4
	sllx	%o0,60,%o5
	xor	%o3,%o1,%o1
	ldub	[%i0+%l7],%l1
	srlx	%o0,4,%o0
	xor	%o1,%o5,%o1
	xor	%o2,%o0,%o0
	and	%o1,0xf,%l5

	ldx	[%l6+%l0],%o3
	sll	%l5,3,%l5
	xor	%o4,%o0,%o0
	ldx	[%i1+%l0],%o2
	srlx	%o1,4,%o1
	ldx	[%l4+%l5],%o4
	sllx	%o0,60,%o5
	srlx	%o0,4,%o0
	and	%l1,0xf0,%l0
	addcc	%l7,-1,%l7
	xor	%o1,%o5,%o1
	and	%l1,0x0f,%l1
	xor	%o3,%o1,%o1
	sll	%l1,4,%l1
	blu	.Lgmult_inner
	and	%o1,0xf,%l5

	ldx	[%l6+%l1],%o3
	sll	%l5,3,%l5
	xor	%o2,%o0,%o0
	ldx	[%i1+%l1],%o2
	srlx	%o1,4,%o1
	xor	%o4,%o0,%o0
	ldx	[%l4+%l5],%o4
	sllx	%o0,60,%o5
	xor	%o3,%o1,%o1
	srlx	%o0,4,%o0
	xor	%o1,%o5,%o1
	xor	%o2,%o0,%o0
	and	%o1,0xf,%l5

	ldx	[%l6+%l0],%o3
	sll	%l5,3,%l5
	xor	%o4,%o0,%o0
	ldx	[%i1+%l0],%o2
	srlx	%o1,4,%o1
	ldx	[%l4+%l5],%o4
	sllx	%o0,60,%o5
	xor	%o3,%o1,%o1
	srlx	%o0,4,%o0
	xor	%o1,%o5,%o1
	xor	%o2,%o0,%o0
	stx	%o1,[%i0+8]
	xor	%o4,%o0,%o0
	stx	%o0,[%i0]

	ret
	restore
.type	gcm_gmult_4bit,#function
.size	gcm_gmult_4bit,(.-gcm_gmult_4bit)
.globl	gcm_init_vis3
.align	32
gcm_init_vis3:
	save	%sp,-192,%sp

	ldx	[%i1+0],%o2
	ldx	[%i1+8],%o1
	mov	0xE1,%o4
	mov	1,%o3
	sllx	%o4,57,%o4
	srax	%o2,63,%g1		! broadcast carry
	addcc	%o1,%o1,%o1		! H<<=1
	.word	0x95b2822a !addxc	%o2,%o2,%o2
	and	%g1,%o3,%o3
	and	%g1,%o4,%o4
	xor	%o3,%o1,%o1
	xor	%o4,%o2,%o2
	stx	%o1,[%i0+8]		! save twisted H
	stx	%o2,[%i0+0]

	sethi	%hi(0xA0406080),%g5
	sethi	%hi(0x20C0E000),%l0
	or	%g5,%lo(0xA0406080),%g5
	or	%l0,%lo(0x20C0E000),%l0
	sllx	%g5,32,%g5
	or	%l0,%g5,%g5		! (0xE0·i)&0xff=0xA040608020C0E000
	stx	%g5,[%i0+16]

	ret
	restore
.type	gcm_init_vis3,#function
.size	gcm_init_vis3,.-gcm_init_vis3

.globl	gcm_gmult_vis3
.align	32
gcm_gmult_vis3:
	save	%sp,-192,%sp

	ldx	[%i0+8],%o3		! load Xi
	ldx	[%i0+0],%o4
	ldx	[%i1+8],%o1	! load twisted H
	ldx	[%i1+0],%o2

	mov	0xE1,%l7
	sllx	%l7,57,%o5		! 57 is not a typo
	ldx	[%i1+16],%g5		! (0xE0·i)&0xff=0xA040608020C0E000

	xor	%o2,%o1,%o0		! Karatsuba pre-processing
	.word	0x83b2e2a9 !xmulx	%o3,%o1,%g1
	xor	%o3,%o4,%g3		! Karatsuba pre-processing
	.word	0x85b0e2a8 !xmulx	%g3,%o0,%g2
	.word	0x97b2e2c9 !xmulxhi	%o3,%o1,%o3
	.word	0x87b0e2c8 !xmulxhi	%g3,%o0,%g3
	.word	0x89b322ca !xmulxhi	%o4,%o2,%g4
	.word	0x99b322aa !xmulx	%o4,%o2,%o4

	sll	%g1,3,%o7
	srlx	%g5,%o7,%o7		! ·0xE0 [implicit &(7<<3)]
	xor	%g1,%o7,%o7
	sllx	%o7,57,%o7		! (%g1·0xE1)<<1<<56 [implicit &0x7f]

	xor	%g1,%g2,%g2		! Karatsuba post-processing
	xor	%o3,%g3,%g3
	 xor	%o7,%o3,%o3		! real destination is %g2
	xor	%g4,%g3,%g3
	xor	%o3,%g2,%g2
	xor	%o4,%g3,%g3
	xor	%o4,%g2,%g2

	.word	0x97b062cd !xmulxhi	%g1,%o5,%o3		! ·0xE1<<1<<56
	 xor	%g1,%g3,%g3
	.word	0x83b0a2ad !xmulx	%g2,%o5,%g1
	 xor	%g2,%g4,%g4
	.word	0x85b0a2cd !xmulxhi	%g2,%o5,%g2

	xor	%o3,%g3,%g3
	xor	%g1,%g3,%g3
	xor	%g2,%g4,%g4

	stx	%g3,[%i0+8]		! save Xi
	stx	%g4,[%i0+0]

	ret
	restore
.type	gcm_gmult_vis3,#function
.size	gcm_gmult_vis3,.-gcm_gmult_vis3

.globl	gcm_ghash_vis3
.align	32
gcm_ghash_vis3:
	save	%sp,-192,%sp
	nop
	srln	%i3,0,%i3		! needed on v8+, "nop" on v9

	ldx	[%i0+8],%g3		! load Xi
	ldx	[%i0+0],%g4
	ldx	[%i1+8],%o1	! load twisted H
	ldx	[%i1+0],%o2

	mov	0xE1,%l7
	sllx	%l7,57,%o5		! 57 is not a typo
	ldx	[%i1+16],%g5		! (0xE0·i)&0xff=0xA040608020C0E000

	and	%i2,7,%l0
	andn	%i2,7,%i2
	sll	%l0,3,%l0
	prefetch [%i2+63], 20
	sub	%g0,%l0,%l1

	xor	%o2,%o1,%o0		! Karatsuba pre-processing
.Loop:
	ldx	[%i2+8],%o3
	brz,pt	%l0,1f
	ldx	[%i2+0],%o4

	ldx	[%i2+16],%g2		! align data
	srlx	%o3,%l1,%g1
	sllx	%o3,%l0,%o3
	sllx	%o4,%l0,%o4
	srlx	%g2,%l1,%g2
	or	%g1,%o4,%o4
	or	%g2,%o3,%o3
1:
	add	%i2,16,%i2
	sub	%i3,16,%i3
	xor	%g3,%o3,%o3
	xor	%g4,%o4,%o4
	prefetch [%i2+63], 20

	.word	0x83b2e2a9 !xmulx	%o3,%o1,%g1
	xor	%o3,%o4,%g3		! Karatsuba pre-processing
	.word	0x85b0e2a8 !xmulx	%g3,%o0,%g2
	.word	0x97b2e2c9 !xmulxhi	%o3,%o1,%o3
	.word	0x87b0e2c8 !xmulxhi	%g3,%o0,%g3
	.word	0x89b322ca !xmulxhi	%o4,%o2,%g4
	.word	0x99b322aa !xmulx	%o4,%o2,%o4

	sll	%g1,3,%o7
	srlx	%g5,%o7,%o7		! ·0xE0 [implicit &(7<<3)]
	xor	%g1,%o7,%o7
	sllx	%o7,57,%o7		! (%g1·0xE1)<<1<<56 [implicit &0x7f]

	xor	%g1,%g2,%g2		! Karatsuba post-processing
	xor	%o3,%g3,%g3
	 xor	%o7,%o3,%o3		! real destination is %g2
	xor	%g4,%g3,%g3
	xor	%o3,%g2,%g2
	xor	%o4,%g3,%g3
	xor	%o4,%g2,%g2

	.word	0x97b062cd !xmulxhi	%g1,%o5,%o3		! ·0xE1<<1<<56
	 xor	%g1,%g3,%g3
	.word	0x83b0a2ad !xmulx	%g2,%o5,%g1
	 xor	%g2,%g4,%g4
	.word	0x85b0a2cd !xmulxhi	%g2,%o5,%g2

	xor	%o3,%g3,%g3
	xor	%g1,%g3,%g3
	brnz,pt	%i3,.Loop
	xor	%g2,%g4,%g4

	stx	%g3,[%i0+8]		! save Xi
	stx	%g4,[%i0+0]

	ret
	restore
.type	gcm_ghash_vis3,#function
.size	gcm_ghash_vis3,.-gcm_ghash_vis3
.asciz	"GHASH for SPARCv9/VIS3, CRYPTOGAMS by <appro@openssl.org>"
.align	4
