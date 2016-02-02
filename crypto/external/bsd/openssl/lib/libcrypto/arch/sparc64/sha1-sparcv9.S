.register	%g2,#scratch
.register	%g3,#scratch
.section	".text",#alloc,#execinstr

.align	32
.globl	sha1_block_data_order
sha1_block_data_order:
	save	%sp,-192,%sp
	sllx	%i2,6,%i2
	add	%i1,%i2,%i2

	or	%g0,1,%g2
	sllx	%g2,32,%g2
	or	%g2,1,%g2

	ld	[%i0+0],%l0
	ld	[%i0+4],%l1
	ld	[%i0+8],%l2
	ld	[%i0+12],%l3
	ld	[%i0+16],%l4
	andn	%i1,7,%i3

	sethi	%hi(0x5a827999),%l5
	or	%l5,%lo(0x5a827999),%l5
	sethi	%hi(0x6ed9eba1),%l6
	or	%l6,%lo(0x6ed9eba1),%l6
	sethi	%hi(0x8f1bbcdc),%l7
	or	%l7,%lo(0x8f1bbcdc),%l7
	sethi	%hi(0xca62c1d6),%g5
	or	%g5,%lo(0xca62c1d6),%g5

.Lloop:
	ldx	[%i3+0],%o0
	ldx	[%i3+16],%o2
	ldx	[%i3+32],%o4
	ldx	[%i3+48],%g1
	and	%i1,7,%i4
	ldx	[%i3+8],%o1
	sll	%i4,3,%i4
	ldx	[%i3+24],%o3
	subcc	%g0,%i4,%i5	! should be 64-%i4, but -%i4 works too
	ldx	[%i3+40],%o5
	bz,pt	%icc,.Laligned
	ldx	[%i3+56],%o7

	sllx	%o0,%i4,%o0
	ldx	[%i3+64],%g3
	srlx	%o1,%i5,%g4
	sllx	%o1,%i4,%o1
	or	%g4,%o0,%o0
	srlx	%o2,%i5,%g4
	sllx	%o2,%i4,%o2
	or	%g4,%o1,%o1
	srlx	%o3,%i5,%g4
	sllx	%o3,%i4,%o3
	or	%g4,%o2,%o2
	srlx	%o4,%i5,%g4
	sllx	%o4,%i4,%o4
	or	%g4,%o3,%o3
	srlx	%o5,%i5,%g4
	sllx	%o5,%i4,%o5
	or	%g4,%o4,%o4
	srlx	%g1,%i5,%g4
	sllx	%g1,%i4,%g1
	or	%g4,%o5,%o5
	srlx	%o7,%i5,%g4
	sllx	%o7,%i4,%o7
	or	%g4,%g1,%g1
	srlx	%g3,%i5,%g3
	or	%g3,%o7,%o7
.Laligned:
	srlx	%o0,32,%g4
	sll	%l0,5,%i3		!! 0
	add	%l5,%l4,%l4
	srl	%l0,27,%i4
	add	%i3,%l4,%l4
	and	%l2,%l1,%i3
	add	%i4,%l4,%l4
	sll	%l1,30,%i5
	andn	%l3,%l1,%i4
	srl	%l1,2,%l1
	or	%i4,%i3,%i4
	or	%i5,%l1,%l1
	add	%g4,%l4,%l4
	add	%i4,%l4,%l4
	sll	%l4,5,%i3		!! 1
	add	%l5,%l3,%l3
	srl	%l4,27,%i4
	add	%i3,%l3,%l3
	and	%l1,%l0,%i3
	add	%i4,%l3,%l3
	sll	%l0,30,%i5
	andn	%l2,%l0,%i4
	srl	%l0,2,%l0
	or	%i4,%i3,%i4
	or	%i5,%l0,%l0
	add	%o0,%l3,%l3
	srlx	%o1,32,%g4
	add	%i4,%l3,%l3
	sll	%l3,5,%i3		!! 2
	add	%l5,%l2,%l2
	srl	%l3,27,%i4
	add	%i3,%l2,%l2
	and	%l0,%l4,%i3
	add	%i4,%l2,%l2
	sll	%l4,30,%i5
	andn	%l1,%l4,%i4
	srl	%l4,2,%l4
	or	%i4,%i3,%i4
	or	%i5,%l4,%l4
	add	%g4,%l2,%l2
	add	%i4,%l2,%l2
	sll	%l2,5,%i3		!! 3
	add	%l5,%l1,%l1
	srl	%l2,27,%i4
	add	%i3,%l1,%l1
	and	%l4,%l3,%i3
	add	%i4,%l1,%l1
	sll	%l3,30,%i5
	andn	%l0,%l3,%i4
	srl	%l3,2,%l3
	or	%i4,%i3,%i4
	or	%i5,%l3,%l3
	add	%o1,%l1,%l1
	srlx	%o2,32,%g4
	add	%i4,%l1,%l1
	sll	%l1,5,%i3		!! 4
	add	%l5,%l0,%l0
	srl	%l1,27,%i4
	add	%i3,%l0,%l0
	and	%l3,%l2,%i3
	add	%i4,%l0,%l0
	sll	%l2,30,%i5
	andn	%l4,%l2,%i4
	srl	%l2,2,%l2
	or	%i4,%i3,%i4
	or	%i5,%l2,%l2
	add	%g4,%l0,%l0
	add	%i4,%l0,%l0
	sll	%l0,5,%i3		!! 5
	add	%l5,%l4,%l4
	srl	%l0,27,%i4
	add	%i3,%l4,%l4
	and	%l2,%l1,%i3
	add	%i4,%l4,%l4
	sll	%l1,30,%i5
	andn	%l3,%l1,%i4
	srl	%l1,2,%l1
	or	%i4,%i3,%i4
	or	%i5,%l1,%l1
	add	%o2,%l4,%l4
	srlx	%o3,32,%g4
	add	%i4,%l4,%l4
	sll	%l4,5,%i3		!! 6
	add	%l5,%l3,%l3
	srl	%l4,27,%i4
	add	%i3,%l3,%l3
	and	%l1,%l0,%i3
	add	%i4,%l3,%l3
	sll	%l0,30,%i5
	andn	%l2,%l0,%i4
	srl	%l0,2,%l0
	or	%i4,%i3,%i4
	or	%i5,%l0,%l0
	add	%g4,%l3,%l3
	add	%i4,%l3,%l3
	sll	%l3,5,%i3		!! 7
	add	%l5,%l2,%l2
	srl	%l3,27,%i4
	add	%i3,%l2,%l2
	and	%l0,%l4,%i3
	add	%i4,%l2,%l2
	sll	%l4,30,%i5
	andn	%l1,%l4,%i4
	srl	%l4,2,%l4
	or	%i4,%i3,%i4
	or	%i5,%l4,%l4
	add	%o3,%l2,%l2
	srlx	%o4,32,%g4
	add	%i4,%l2,%l2
	sll	%l2,5,%i3		!! 8
	add	%l5,%l1,%l1
	srl	%l2,27,%i4
	add	%i3,%l1,%l1
	and	%l4,%l3,%i3
	add	%i4,%l1,%l1
	sll	%l3,30,%i5
	andn	%l0,%l3,%i4
	srl	%l3,2,%l3
	or	%i4,%i3,%i4
	or	%i5,%l3,%l3
	add	%g4,%l1,%l1
	add	%i4,%l1,%l1
	sll	%l1,5,%i3		!! 9
	add	%l5,%l0,%l0
	srl	%l1,27,%i4
	add	%i3,%l0,%l0
	and	%l3,%l2,%i3
	add	%i4,%l0,%l0
	sll	%l2,30,%i5
	andn	%l4,%l2,%i4
	srl	%l2,2,%l2
	or	%i4,%i3,%i4
	or	%i5,%l2,%l2
	add	%o4,%l0,%l0
	srlx	%o5,32,%g4
	add	%i4,%l0,%l0
	sll	%l0,5,%i3		!! 10
	add	%l5,%l4,%l4
	srl	%l0,27,%i4
	add	%i3,%l4,%l4
	and	%l2,%l1,%i3
	add	%i4,%l4,%l4
	sll	%l1,30,%i5
	andn	%l3,%l1,%i4
	srl	%l1,2,%l1
	or	%i4,%i3,%i4
	or	%i5,%l1,%l1
	add	%g4,%l4,%l4
	add	%i4,%l4,%l4
	sll	%l4,5,%i3		!! 11
	add	%l5,%l3,%l3
	srl	%l4,27,%i4
	add	%i3,%l3,%l3
	and	%l1,%l0,%i3
	add	%i4,%l3,%l3
	sll	%l0,30,%i5
	andn	%l2,%l0,%i4
	srl	%l0,2,%l0
	or	%i4,%i3,%i4
	or	%i5,%l0,%l0
	add	%o5,%l3,%l3
	srlx	%g1,32,%g4
	add	%i4,%l3,%l3
	sll	%l3,5,%i3		!! 12
	add	%l5,%l2,%l2
	srl	%l3,27,%i4
	add	%i3,%l2,%l2
	and	%l0,%l4,%i3
	add	%i4,%l2,%l2
	sll	%l4,30,%i5
	andn	%l1,%l4,%i4
	srl	%l4,2,%l4
	or	%i4,%i3,%i4
	or	%i5,%l4,%l4
	add	%g4,%l2,%l2
	add	%i4,%l2,%l2
	sll	%l2,5,%i3		!! 13
	add	%l5,%l1,%l1
	srl	%l2,27,%i4
	add	%i3,%l1,%l1
	and	%l4,%l3,%i3
	add	%i4,%l1,%l1
	sll	%l3,30,%i5
	andn	%l0,%l3,%i4
	srl	%l3,2,%l3
	or	%i4,%i3,%i4
	or	%i5,%l3,%l3
	add	%g1,%l1,%l1
	srlx	%o7,32,%g4
	add	%i4,%l1,%l1
	sll	%l1,5,%i3		!! 14
	add	%l5,%l0,%l0
	srl	%l1,27,%i4
	add	%i3,%l0,%l0
	and	%l3,%l2,%i3
	add	%i4,%l0,%l0
	sll	%l2,30,%i5
	andn	%l4,%l2,%i4
	srl	%l2,2,%l2
	or	%i4,%i3,%i4
	or	%i5,%l2,%l2
	add	%g4,%l0,%l0
	add	%i4,%l0,%l0
	sll	%l0,5,%i3		!! 15
	add	%l5,%l4,%l4
	srl	%l0,27,%i4
	add	%i3,%l4,%l4
	and	%l2,%l1,%i3
	add	%i4,%l4,%l4
	sll	%l1,30,%i5
	andn	%l3,%l1,%i4
	srl	%l1,2,%l1
	or	%i4,%i3,%i4
	or	%i5,%l1,%l1
	add	%o7,%l4,%l4
	add	%i4,%l4,%l4
	sllx	%g1,32,%g4	! Xupdate(16)
	xor	%o1,%o0,%o0
	srlx	%o7,32,%i4
	xor	%o4,%o0,%o0
	sll	%l4,5,%i3		!! 16
	or	%i4,%g4,%g4
	add	%l5,%l3,%l3		!!
	xor	%g4,%o0,%o0
	srlx	%o0,31,%g4
	add	%o0,%o0,%o0
	and	%g4,%g2,%g4
	andn	%o0,%g2,%o0
	srl	%l4,27,%i4		!!
	or	%g4,%o0,%o0
	srlx	%o0,32,%g4
	add	%i3,%l3,%l3		!!
	and	%l1,%l0,%i3
	add	%i4,%l3,%l3
	sll	%l0,30,%i5
	add	%g4,%l3,%l3
	andn	%l2,%l0,%i4
	srl	%l0,2,%l0
	or	%i4,%i3,%i4
	or	%i5,%l0,%l0
	add	%i4,%l3,%l3
	sll	%l3,5,%i3		!! 17
	add	%l5,%l2,%l2
	srl	%l3,27,%i4
	add	%i3,%l2,%l2		!!
	and	%l0,%l4,%i3
	add	%i4,%l2,%l2
	sll	%l4,30,%i5
	add	%o0,%l2,%l2
	andn	%l1,%l4,%i4
	srl	%l4,2,%l4
	or	%i4,%i3,%i4
	or	%i5,%l4,%l4
	add	%i4,%l2,%l2
	sllx	%o7,32,%g4	! Xupdate(18)
	xor	%o2,%o1,%o1
	srlx	%o0,32,%i4
	xor	%o5,%o1,%o1
	sll	%l2,5,%i3		!! 18
	or	%i4,%g4,%g4
	add	%l5,%l1,%l1		!!
	xor	%g4,%o1,%o1
	srlx	%o1,31,%g4
	add	%o1,%o1,%o1
	and	%g4,%g2,%g4
	andn	%o1,%g2,%o1
	srl	%l2,27,%i4		!!
	or	%g4,%o1,%o1
	srlx	%o1,32,%g4
	add	%i3,%l1,%l1		!!
	and	%l4,%l3,%i3
	add	%i4,%l1,%l1
	sll	%l3,30,%i5
	add	%g4,%l1,%l1
	andn	%l0,%l3,%i4
	srl	%l3,2,%l3
	or	%i4,%i3,%i4
	or	%i5,%l3,%l3
	add	%i4,%l1,%l1
	sll	%l1,5,%i3		!! 19
	add	%l5,%l0,%l0
	srl	%l1,27,%i4
	add	%i3,%l0,%l0		!!
	and	%l3,%l2,%i3
	add	%i4,%l0,%l0
	sll	%l2,30,%i5
	add	%o1,%l0,%l0
	andn	%l4,%l2,%i4
	srl	%l2,2,%l2
	or	%i4,%i3,%i4
	or	%i5,%l2,%l2
	add	%i4,%l0,%l0
	sllx	%o0,32,%g4	! Xupdate(20)
	xor	%o3,%o2,%o2
	srlx	%o1,32,%i4
	xor	%g1,%o2,%o2
	sll	%l0,5,%i3		!! 20
	or	%i4,%g4,%g4
	add	%l6,%l4,%l4		!!
	xor	%g4,%o2,%o2
	srlx	%o2,31,%g4
	add	%o2,%o2,%o2
	and	%g4,%g2,%g4
	andn	%o2,%g2,%o2
	srl	%l0,27,%i4		!!
	or	%g4,%o2,%o2
	srlx	%o2,32,%g4
	add	%i3,%l4,%l4		!!
	xor	%l2,%l1,%i3
	add	%i4,%l4,%l4
	sll	%l1,30,%i5
	xor	%l3,%i3,%i4
	srl	%l1,2,%l1
	add	%i4,%l4,%l4
	or	%i5,%l1,%l1
	add	%g4,%l4,%l4
	sll	%l4,5,%i3		!! 21
	add	%l6,%l3,%l3
	srl	%l4,27,%i4
	add	%i3,%l3,%l3		!!
	xor	%l1,%l0,%i3
	add	%i4,%l3,%l3
	sll	%l0,30,%i5
	xor	%l2,%i3,%i4
	srl	%l0,2,%l0
	add	%i4,%l3,%l3
	or	%i5,%l0,%l0
	add	%o2,%l3,%l3
	sllx	%o1,32,%g4	! Xupdate(22)
	xor	%o4,%o3,%o3
	srlx	%o2,32,%i4
	xor	%o7,%o3,%o3
	sll	%l3,5,%i3		!! 22
	or	%i4,%g4,%g4
	add	%l6,%l2,%l2		!!
	xor	%g4,%o3,%o3
	srlx	%o3,31,%g4
	add	%o3,%o3,%o3
	and	%g4,%g2,%g4
	andn	%o3,%g2,%o3
	srl	%l3,27,%i4		!!
	or	%g4,%o3,%o3
	srlx	%o3,32,%g4
	add	%i3,%l2,%l2		!!
	xor	%l0,%l4,%i3
	add	%i4,%l2,%l2
	sll	%l4,30,%i5
	xor	%l1,%i3,%i4
	srl	%l4,2,%l4
	add	%i4,%l2,%l2
	or	%i5,%l4,%l4
	add	%g4,%l2,%l2
	sll	%l2,5,%i3		!! 23
	add	%l6,%l1,%l1
	srl	%l2,27,%i4
	add	%i3,%l1,%l1		!!
	xor	%l4,%l3,%i3
	add	%i4,%l1,%l1
	sll	%l3,30,%i5
	xor	%l0,%i3,%i4
	srl	%l3,2,%l3
	add	%i4,%l1,%l1
	or	%i5,%l3,%l3
	add	%o3,%l1,%l1
	sllx	%o2,32,%g4	! Xupdate(24)
	xor	%o5,%o4,%o4
	srlx	%o3,32,%i4
	xor	%o0,%o4,%o4
	sll	%l1,5,%i3		!! 24
	or	%i4,%g4,%g4
	add	%l6,%l0,%l0		!!
	xor	%g4,%o4,%o4
	srlx	%o4,31,%g4
	add	%o4,%o4,%o4
	and	%g4,%g2,%g4
	andn	%o4,%g2,%o4
	srl	%l1,27,%i4		!!
	or	%g4,%o4,%o4
	srlx	%o4,32,%g4
	add	%i3,%l0,%l0		!!
	xor	%l3,%l2,%i3
	add	%i4,%l0,%l0
	sll	%l2,30,%i5
	xor	%l4,%i3,%i4
	srl	%l2,2,%l2
	add	%i4,%l0,%l0
	or	%i5,%l2,%l2
	add	%g4,%l0,%l0
	sll	%l0,5,%i3		!! 25
	add	%l6,%l4,%l4
	srl	%l0,27,%i4
	add	%i3,%l4,%l4		!!
	xor	%l2,%l1,%i3
	add	%i4,%l4,%l4
	sll	%l1,30,%i5
	xor	%l3,%i3,%i4
	srl	%l1,2,%l1
	add	%i4,%l4,%l4
	or	%i5,%l1,%l1
	add	%o4,%l4,%l4
	sllx	%o3,32,%g4	! Xupdate(26)
	xor	%g1,%o5,%o5
	srlx	%o4,32,%i4
	xor	%o1,%o5,%o5
	sll	%l4,5,%i3		!! 26
	or	%i4,%g4,%g4
	add	%l6,%l3,%l3		!!
	xor	%g4,%o5,%o5
	srlx	%o5,31,%g4
	add	%o5,%o5,%o5
	and	%g4,%g2,%g4
	andn	%o5,%g2,%o5
	srl	%l4,27,%i4		!!
	or	%g4,%o5,%o5
	srlx	%o5,32,%g4
	add	%i3,%l3,%l3		!!
	xor	%l1,%l0,%i3
	add	%i4,%l3,%l3
	sll	%l0,30,%i5
	xor	%l2,%i3,%i4
	srl	%l0,2,%l0
	add	%i4,%l3,%l3
	or	%i5,%l0,%l0
	add	%g4,%l3,%l3
	sll	%l3,5,%i3		!! 27
	add	%l6,%l2,%l2
	srl	%l3,27,%i4
	add	%i3,%l2,%l2		!!
	xor	%l0,%l4,%i3
	add	%i4,%l2,%l2
	sll	%l4,30,%i5
	xor	%l1,%i3,%i4
	srl	%l4,2,%l4
	add	%i4,%l2,%l2
	or	%i5,%l4,%l4
	add	%o5,%l2,%l2
	sllx	%o4,32,%g4	! Xupdate(28)
	xor	%o7,%g1,%g1
	srlx	%o5,32,%i4
	xor	%o2,%g1,%g1
	sll	%l2,5,%i3		!! 28
	or	%i4,%g4,%g4
	add	%l6,%l1,%l1		!!
	xor	%g4,%g1,%g1
	srlx	%g1,31,%g4
	add	%g1,%g1,%g1
	and	%g4,%g2,%g4
	andn	%g1,%g2,%g1
	srl	%l2,27,%i4		!!
	or	%g4,%g1,%g1
	srlx	%g1,32,%g4
	add	%i3,%l1,%l1		!!
	xor	%l4,%l3,%i3
	add	%i4,%l1,%l1
	sll	%l3,30,%i5
	xor	%l0,%i3,%i4
	srl	%l3,2,%l3
	add	%i4,%l1,%l1
	or	%i5,%l3,%l3
	add	%g4,%l1,%l1
	sll	%l1,5,%i3		!! 29
	add	%l6,%l0,%l0
	srl	%l1,27,%i4
	add	%i3,%l0,%l0		!!
	xor	%l3,%l2,%i3
	add	%i4,%l0,%l0
	sll	%l2,30,%i5
	xor	%l4,%i3,%i4
	srl	%l2,2,%l2
	add	%i4,%l0,%l0
	or	%i5,%l2,%l2
	add	%g1,%l0,%l0
	sllx	%o5,32,%g4	! Xupdate(30)
	xor	%o0,%o7,%o7
	srlx	%g1,32,%i4
	xor	%o3,%o7,%o7
	sll	%l0,5,%i3		!! 30
	or	%i4,%g4,%g4
	add	%l6,%l4,%l4		!!
	xor	%g4,%o7,%o7
	srlx	%o7,31,%g4
	add	%o7,%o7,%o7
	and	%g4,%g2,%g4
	andn	%o7,%g2,%o7
	srl	%l0,27,%i4		!!
	or	%g4,%o7,%o7
	srlx	%o7,32,%g4
	add	%i3,%l4,%l4		!!
	xor	%l2,%l1,%i3
	add	%i4,%l4,%l4
	sll	%l1,30,%i5
	xor	%l3,%i3,%i4
	srl	%l1,2,%l1
	add	%i4,%l4,%l4
	or	%i5,%l1,%l1
	add	%g4,%l4,%l4
	sll	%l4,5,%i3		!! 31
	add	%l6,%l3,%l3
	srl	%l4,27,%i4
	add	%i3,%l3,%l3		!!
	xor	%l1,%l0,%i3
	add	%i4,%l3,%l3
	sll	%l0,30,%i5
	xor	%l2,%i3,%i4
	srl	%l0,2,%l0
	add	%i4,%l3,%l3
	or	%i5,%l0,%l0
	add	%o7,%l3,%l3
	sllx	%g1,32,%g4	! Xupdate(32)
	xor	%o1,%o0,%o0
	srlx	%o7,32,%i4
	xor	%o4,%o0,%o0
	sll	%l3,5,%i3		!! 32
	or	%i4,%g4,%g4
	add	%l6,%l2,%l2		!!
	xor	%g4,%o0,%o0
	srlx	%o0,31,%g4
	add	%o0,%o0,%o0
	and	%g4,%g2,%g4
	andn	%o0,%g2,%o0
	srl	%l3,27,%i4		!!
	or	%g4,%o0,%o0
	srlx	%o0,32,%g4
	add	%i3,%l2,%l2		!!
	xor	%l0,%l4,%i3
	add	%i4,%l2,%l2
	sll	%l4,30,%i5
	xor	%l1,%i3,%i4
	srl	%l4,2,%l4
	add	%i4,%l2,%l2
	or	%i5,%l4,%l4
	add	%g4,%l2,%l2
	sll	%l2,5,%i3		!! 33
	add	%l6,%l1,%l1
	srl	%l2,27,%i4
	add	%i3,%l1,%l1		!!
	xor	%l4,%l3,%i3
	add	%i4,%l1,%l1
	sll	%l3,30,%i5
	xor	%l0,%i3,%i4
	srl	%l3,2,%l3
	add	%i4,%l1,%l1
	or	%i5,%l3,%l3
	add	%o0,%l1,%l1
	sllx	%o7,32,%g4	! Xupdate(34)
	xor	%o2,%o1,%o1
	srlx	%o0,32,%i4
	xor	%o5,%o1,%o1
	sll	%l1,5,%i3		!! 34
	or	%i4,%g4,%g4
	add	%l6,%l0,%l0		!!
	xor	%g4,%o1,%o1
	srlx	%o1,31,%g4
	add	%o1,%o1,%o1
	and	%g4,%g2,%g4
	andn	%o1,%g2,%o1
	srl	%l1,27,%i4		!!
	or	%g4,%o1,%o1
	srlx	%o1,32,%g4
	add	%i3,%l0,%l0		!!
	xor	%l3,%l2,%i3
	add	%i4,%l0,%l0
	sll	%l2,30,%i5
	xor	%l4,%i3,%i4
	srl	%l2,2,%l2
	add	%i4,%l0,%l0
	or	%i5,%l2,%l2
	add	%g4,%l0,%l0
	sll	%l0,5,%i3		!! 35
	add	%l6,%l4,%l4
	srl	%l0,27,%i4
	add	%i3,%l4,%l4		!!
	xor	%l2,%l1,%i3
	add	%i4,%l4,%l4
	sll	%l1,30,%i5
	xor	%l3,%i3,%i4
	srl	%l1,2,%l1
	add	%i4,%l4,%l4
	or	%i5,%l1,%l1
	add	%o1,%l4,%l4
	sllx	%o0,32,%g4	! Xupdate(36)
	xor	%o3,%o2,%o2
	srlx	%o1,32,%i4
	xor	%g1,%o2,%o2
	sll	%l4,5,%i3		!! 36
	or	%i4,%g4,%g4
	add	%l6,%l3,%l3		!!
	xor	%g4,%o2,%o2
	srlx	%o2,31,%g4
	add	%o2,%o2,%o2
	and	%g4,%g2,%g4
	andn	%o2,%g2,%o2
	srl	%l4,27,%i4		!!
	or	%g4,%o2,%o2
	srlx	%o2,32,%g4
	add	%i3,%l3,%l3		!!
	xor	%l1,%l0,%i3
	add	%i4,%l3,%l3
	sll	%l0,30,%i5
	xor	%l2,%i3,%i4
	srl	%l0,2,%l0
	add	%i4,%l3,%l3
	or	%i5,%l0,%l0
	add	%g4,%l3,%l3
	sll	%l3,5,%i3		!! 37
	add	%l6,%l2,%l2
	srl	%l3,27,%i4
	add	%i3,%l2,%l2		!!
	xor	%l0,%l4,%i3
	add	%i4,%l2,%l2
	sll	%l4,30,%i5
	xor	%l1,%i3,%i4
	srl	%l4,2,%l4
	add	%i4,%l2,%l2
	or	%i5,%l4,%l4
	add	%o2,%l2,%l2
	sllx	%o1,32,%g4	! Xupdate(38)
	xor	%o4,%o3,%o3
	srlx	%o2,32,%i4
	xor	%o7,%o3,%o3
	sll	%l2,5,%i3		!! 38
	or	%i4,%g4,%g4
	add	%l6,%l1,%l1		!!
	xor	%g4,%o3,%o3
	srlx	%o3,31,%g4
	add	%o3,%o3,%o3
	and	%g4,%g2,%g4
	andn	%o3,%g2,%o3
	srl	%l2,27,%i4		!!
	or	%g4,%o3,%o3
	srlx	%o3,32,%g4
	add	%i3,%l1,%l1		!!
	xor	%l4,%l3,%i3
	add	%i4,%l1,%l1
	sll	%l3,30,%i5
	xor	%l0,%i3,%i4
	srl	%l3,2,%l3
	add	%i4,%l1,%l1
	or	%i5,%l3,%l3
	add	%g4,%l1,%l1
	sll	%l1,5,%i3		!! 39
	add	%l6,%l0,%l0
	srl	%l1,27,%i4
	add	%i3,%l0,%l0		!!
	xor	%l3,%l2,%i3
	add	%i4,%l0,%l0
	sll	%l2,30,%i5
	xor	%l4,%i3,%i4
	srl	%l2,2,%l2
	add	%i4,%l0,%l0
	or	%i5,%l2,%l2
	add	%o3,%l0,%l0
	sllx	%o2,32,%g4	! Xupdate(40)
	xor	%o5,%o4,%o4
	srlx	%o3,32,%i4
	xor	%o0,%o4,%o4
	sll	%l0,5,%i3		!! 40
	or	%i4,%g4,%g4
	add	%l7,%l4,%l4		!!
	xor	%g4,%o4,%o4
	srlx	%o4,31,%g4
	add	%o4,%o4,%o4
	and	%g4,%g2,%g4
	andn	%o4,%g2,%o4
	srl	%l0,27,%i4		!!
	or	%g4,%o4,%o4
	srlx	%o4,32,%g4
	add	%i3,%l4,%l4		!!
	and	%l2,%l1,%i3
	add	%i4,%l4,%l4
	sll	%l1,30,%i5
	or	%l2,%l1,%i4
	srl	%l1,2,%l1
	and	%l3,%i4,%i4
	add	%g4,%l4,%l4
	or	%i4,%i3,%i4
	or	%i5,%l1,%l1
	add	%i4,%l4,%l4
	sll	%l4,5,%i3		!! 41
	add	%l7,%l3,%l3
	srl	%l4,27,%i4
	add	%i3,%l3,%l3		!!
	and	%l1,%l0,%i3
	add	%i4,%l3,%l3
	sll	%l0,30,%i5
	or	%l1,%l0,%i4
	srl	%l0,2,%l0
	and	%l2,%i4,%i4
	add	%o4,%l3,%l3
	or	%i4,%i3,%i4
	or	%i5,%l0,%l0
	add	%i4,%l3,%l3
	sllx	%o3,32,%g4	! Xupdate(42)
	xor	%g1,%o5,%o5
	srlx	%o4,32,%i4
	xor	%o1,%o5,%o5
	sll	%l3,5,%i3		!! 42
	or	%i4,%g4,%g4
	add	%l7,%l2,%l2		!!
	xor	%g4,%o5,%o5
	srlx	%o5,31,%g4
	add	%o5,%o5,%o5
	and	%g4,%g2,%g4
	andn	%o5,%g2,%o5
	srl	%l3,27,%i4		!!
	or	%g4,%o5,%o5
	srlx	%o5,32,%g4
	add	%i3,%l2,%l2		!!
	and	%l0,%l4,%i3
	add	%i4,%l2,%l2
	sll	%l4,30,%i5
	or	%l0,%l4,%i4
	srl	%l4,2,%l4
	and	%l1,%i4,%i4
	add	%g4,%l2,%l2
	or	%i4,%i3,%i4
	or	%i5,%l4,%l4
	add	%i4,%l2,%l2
	sll	%l2,5,%i3		!! 43
	add	%l7,%l1,%l1
	srl	%l2,27,%i4
	add	%i3,%l1,%l1		!!
	and	%l4,%l3,%i3
	add	%i4,%l1,%l1
	sll	%l3,30,%i5
	or	%l4,%l3,%i4
	srl	%l3,2,%l3
	and	%l0,%i4,%i4
	add	%o5,%l1,%l1
	or	%i4,%i3,%i4
	or	%i5,%l3,%l3
	add	%i4,%l1,%l1
	sllx	%o4,32,%g4	! Xupdate(44)
	xor	%o7,%g1,%g1
	srlx	%o5,32,%i4
	xor	%o2,%g1,%g1
	sll	%l1,5,%i3		!! 44
	or	%i4,%g4,%g4
	add	%l7,%l0,%l0		!!
	xor	%g4,%g1,%g1
	srlx	%g1,31,%g4
	add	%g1,%g1,%g1
	and	%g4,%g2,%g4
	andn	%g1,%g2,%g1
	srl	%l1,27,%i4		!!
	or	%g4,%g1,%g1
	srlx	%g1,32,%g4
	add	%i3,%l0,%l0		!!
	and	%l3,%l2,%i3
	add	%i4,%l0,%l0
	sll	%l2,30,%i5
	or	%l3,%l2,%i4
	srl	%l2,2,%l2
	and	%l4,%i4,%i4
	add	%g4,%l0,%l0
	or	%i4,%i3,%i4
	or	%i5,%l2,%l2
	add	%i4,%l0,%l0
	sll	%l0,5,%i3		!! 45
	add	%l7,%l4,%l4
	srl	%l0,27,%i4
	add	%i3,%l4,%l4		!!
	and	%l2,%l1,%i3
	add	%i4,%l4,%l4
	sll	%l1,30,%i5
	or	%l2,%l1,%i4
	srl	%l1,2,%l1
	and	%l3,%i4,%i4
	add	%g1,%l4,%l4
	or	%i4,%i3,%i4
	or	%i5,%l1,%l1
	add	%i4,%l4,%l4
	sllx	%o5,32,%g4	! Xupdate(46)
	xor	%o0,%o7,%o7
	srlx	%g1,32,%i4
	xor	%o3,%o7,%o7
	sll	%l4,5,%i3		!! 46
	or	%i4,%g4,%g4
	add	%l7,%l3,%l3		!!
	xor	%g4,%o7,%o7
	srlx	%o7,31,%g4
	add	%o7,%o7,%o7
	and	%g4,%g2,%g4
	andn	%o7,%g2,%o7
	srl	%l4,27,%i4		!!
	or	%g4,%o7,%o7
	srlx	%o7,32,%g4
	add	%i3,%l3,%l3		!!
	and	%l1,%l0,%i3
	add	%i4,%l3,%l3
	sll	%l0,30,%i5
	or	%l1,%l0,%i4
	srl	%l0,2,%l0
	and	%l2,%i4,%i4
	add	%g4,%l3,%l3
	or	%i4,%i3,%i4
	or	%i5,%l0,%l0
	add	%i4,%l3,%l3
	sll	%l3,5,%i3		!! 47
	add	%l7,%l2,%l2
	srl	%l3,27,%i4
	add	%i3,%l2,%l2		!!
	and	%l0,%l4,%i3
	add	%i4,%l2,%l2
	sll	%l4,30,%i5
	or	%l0,%l4,%i4
	srl	%l4,2,%l4
	and	%l1,%i4,%i4
	add	%o7,%l2,%l2
	or	%i4,%i3,%i4
	or	%i5,%l4,%l4
	add	%i4,%l2,%l2
	sllx	%g1,32,%g4	! Xupdate(48)
	xor	%o1,%o0,%o0
	srlx	%o7,32,%i4
	xor	%o4,%o0,%o0
	sll	%l2,5,%i3		!! 48
	or	%i4,%g4,%g4
	add	%l7,%l1,%l1		!!
	xor	%g4,%o0,%o0
	srlx	%o0,31,%g4
	add	%o0,%o0,%o0
	and	%g4,%g2,%g4
	andn	%o0,%g2,%o0
	srl	%l2,27,%i4		!!
	or	%g4,%o0,%o0
	srlx	%o0,32,%g4
	add	%i3,%l1,%l1		!!
	and	%l4,%l3,%i3
	add	%i4,%l1,%l1
	sll	%l3,30,%i5
	or	%l4,%l3,%i4
	srl	%l3,2,%l3
	and	%l0,%i4,%i4
	add	%g4,%l1,%l1
	or	%i4,%i3,%i4
	or	%i5,%l3,%l3
	add	%i4,%l1,%l1
	sll	%l1,5,%i3		!! 49
	add	%l7,%l0,%l0
	srl	%l1,27,%i4
	add	%i3,%l0,%l0		!!
	and	%l3,%l2,%i3
	add	%i4,%l0,%l0
	sll	%l2,30,%i5
	or	%l3,%l2,%i4
	srl	%l2,2,%l2
	and	%l4,%i4,%i4
	add	%o0,%l0,%l0
	or	%i4,%i3,%i4
	or	%i5,%l2,%l2
	add	%i4,%l0,%l0
	sllx	%o7,32,%g4	! Xupdate(50)
	xor	%o2,%o1,%o1
	srlx	%o0,32,%i4
	xor	%o5,%o1,%o1
	sll	%l0,5,%i3		!! 50
	or	%i4,%g4,%g4
	add	%l7,%l4,%l4		!!
	xor	%g4,%o1,%o1
	srlx	%o1,31,%g4
	add	%o1,%o1,%o1
	and	%g4,%g2,%g4
	andn	%o1,%g2,%o1
	srl	%l0,27,%i4		!!
	or	%g4,%o1,%o1
	srlx	%o1,32,%g4
	add	%i3,%l4,%l4		!!
	and	%l2,%l1,%i3
	add	%i4,%l4,%l4
	sll	%l1,30,%i5
	or	%l2,%l1,%i4
	srl	%l1,2,%l1
	and	%l3,%i4,%i4
	add	%g4,%l4,%l4
	or	%i4,%i3,%i4
	or	%i5,%l1,%l1
	add	%i4,%l4,%l4
	sll	%l4,5,%i3		!! 51
	add	%l7,%l3,%l3
	srl	%l4,27,%i4
	add	%i3,%l3,%l3		!!
	and	%l1,%l0,%i3
	add	%i4,%l3,%l3
	sll	%l0,30,%i5
	or	%l1,%l0,%i4
	srl	%l0,2,%l0
	and	%l2,%i4,%i4
	add	%o1,%l3,%l3
	or	%i4,%i3,%i4
	or	%i5,%l0,%l0
	add	%i4,%l3,%l3
	sllx	%o0,32,%g4	! Xupdate(52)
	xor	%o3,%o2,%o2
	srlx	%o1,32,%i4
	xor	%g1,%o2,%o2
	sll	%l3,5,%i3		!! 52
	or	%i4,%g4,%g4
	add	%l7,%l2,%l2		!!
	xor	%g4,%o2,%o2
	srlx	%o2,31,%g4
	add	%o2,%o2,%o2
	and	%g4,%g2,%g4
	andn	%o2,%g2,%o2
	srl	%l3,27,%i4		!!
	or	%g4,%o2,%o2
	srlx	%o2,32,%g4
	add	%i3,%l2,%l2		!!
	and	%l0,%l4,%i3
	add	%i4,%l2,%l2
	sll	%l4,30,%i5
	or	%l0,%l4,%i4
	srl	%l4,2,%l4
	and	%l1,%i4,%i4
	add	%g4,%l2,%l2
	or	%i4,%i3,%i4
	or	%i5,%l4,%l4
	add	%i4,%l2,%l2
	sll	%l2,5,%i3		!! 53
	add	%l7,%l1,%l1
	srl	%l2,27,%i4
	add	%i3,%l1,%l1		!!
	and	%l4,%l3,%i3
	add	%i4,%l1,%l1
	sll	%l3,30,%i5
	or	%l4,%l3,%i4
	srl	%l3,2,%l3
	and	%l0,%i4,%i4
	add	%o2,%l1,%l1
	or	%i4,%i3,%i4
	or	%i5,%l3,%l3
	add	%i4,%l1,%l1
	sllx	%o1,32,%g4	! Xupdate(54)
	xor	%o4,%o3,%o3
	srlx	%o2,32,%i4
	xor	%o7,%o3,%o3
	sll	%l1,5,%i3		!! 54
	or	%i4,%g4,%g4
	add	%l7,%l0,%l0		!!
	xor	%g4,%o3,%o3
	srlx	%o3,31,%g4
	add	%o3,%o3,%o3
	and	%g4,%g2,%g4
	andn	%o3,%g2,%o3
	srl	%l1,27,%i4		!!
	or	%g4,%o3,%o3
	srlx	%o3,32,%g4
	add	%i3,%l0,%l0		!!
	and	%l3,%l2,%i3
	add	%i4,%l0,%l0
	sll	%l2,30,%i5
	or	%l3,%l2,%i4
	srl	%l2,2,%l2
	and	%l4,%i4,%i4
	add	%g4,%l0,%l0
	or	%i4,%i3,%i4
	or	%i5,%l2,%l2
	add	%i4,%l0,%l0
	sll	%l0,5,%i3		!! 55
	add	%l7,%l4,%l4
	srl	%l0,27,%i4
	add	%i3,%l4,%l4		!!
	and	%l2,%l1,%i3
	add	%i4,%l4,%l4
	sll	%l1,30,%i5
	or	%l2,%l1,%i4
	srl	%l1,2,%l1
	and	%l3,%i4,%i4
	add	%o3,%l4,%l4
	or	%i4,%i3,%i4
	or	%i5,%l1,%l1
	add	%i4,%l4,%l4
	sllx	%o2,32,%g4	! Xupdate(56)
	xor	%o5,%o4,%o4
	srlx	%o3,32,%i4
	xor	%o0,%o4,%o4
	sll	%l4,5,%i3		!! 56
	or	%i4,%g4,%g4
	add	%l7,%l3,%l3		!!
	xor	%g4,%o4,%o4
	srlx	%o4,31,%g4
	add	%o4,%o4,%o4
	and	%g4,%g2,%g4
	andn	%o4,%g2,%o4
	srl	%l4,27,%i4		!!
	or	%g4,%o4,%o4
	srlx	%o4,32,%g4
	add	%i3,%l3,%l3		!!
	and	%l1,%l0,%i3
	add	%i4,%l3,%l3
	sll	%l0,30,%i5
	or	%l1,%l0,%i4
	srl	%l0,2,%l0
	and	%l2,%i4,%i4
	add	%g4,%l3,%l3
	or	%i4,%i3,%i4
	or	%i5,%l0,%l0
	add	%i4,%l3,%l3
	sll	%l3,5,%i3		!! 57
	add	%l7,%l2,%l2
	srl	%l3,27,%i4
	add	%i3,%l2,%l2		!!
	and	%l0,%l4,%i3
	add	%i4,%l2,%l2
	sll	%l4,30,%i5
	or	%l0,%l4,%i4
	srl	%l4,2,%l4
	and	%l1,%i4,%i4
	add	%o4,%l2,%l2
	or	%i4,%i3,%i4
	or	%i5,%l4,%l4
	add	%i4,%l2,%l2
	sllx	%o3,32,%g4	! Xupdate(58)
	xor	%g1,%o5,%o5
	srlx	%o4,32,%i4
	xor	%o1,%o5,%o5
	sll	%l2,5,%i3		!! 58
	or	%i4,%g4,%g4
	add	%l7,%l1,%l1		!!
	xor	%g4,%o5,%o5
	srlx	%o5,31,%g4
	add	%o5,%o5,%o5
	and	%g4,%g2,%g4
	andn	%o5,%g2,%o5
	srl	%l2,27,%i4		!!
	or	%g4,%o5,%o5
	srlx	%o5,32,%g4
	add	%i3,%l1,%l1		!!
	and	%l4,%l3,%i3
	add	%i4,%l1,%l1
	sll	%l3,30,%i5
	or	%l4,%l3,%i4
	srl	%l3,2,%l3
	and	%l0,%i4,%i4
	add	%g4,%l1,%l1
	or	%i4,%i3,%i4
	or	%i5,%l3,%l3
	add	%i4,%l1,%l1
	sll	%l1,5,%i3		!! 59
	add	%l7,%l0,%l0
	srl	%l1,27,%i4
	add	%i3,%l0,%l0		!!
	and	%l3,%l2,%i3
	add	%i4,%l0,%l0
	sll	%l2,30,%i5
	or	%l3,%l2,%i4
	srl	%l2,2,%l2
	and	%l4,%i4,%i4
	add	%o5,%l0,%l0
	or	%i4,%i3,%i4
	or	%i5,%l2,%l2
	add	%i4,%l0,%l0
	sllx	%o4,32,%g4	! Xupdate(60)
	xor	%o7,%g1,%g1
	srlx	%o5,32,%i4
	xor	%o2,%g1,%g1
	sll	%l0,5,%i3		!! 60
	or	%i4,%g4,%g4
	add	%g5,%l4,%l4		!!
	xor	%g4,%g1,%g1
	srlx	%g1,31,%g4
	add	%g1,%g1,%g1
	and	%g4,%g2,%g4
	andn	%g1,%g2,%g1
	srl	%l0,27,%i4		!!
	or	%g4,%g1,%g1
	srlx	%g1,32,%g4
	add	%i3,%l4,%l4		!!
	xor	%l2,%l1,%i3
	add	%i4,%l4,%l4
	sll	%l1,30,%i5
	xor	%l3,%i3,%i4
	srl	%l1,2,%l1
	add	%i4,%l4,%l4
	or	%i5,%l1,%l1
	add	%g4,%l4,%l4
	sll	%l4,5,%i3		!! 61
	add	%g5,%l3,%l3
	srl	%l4,27,%i4
	add	%i3,%l3,%l3		!!
	xor	%l1,%l0,%i3
	add	%i4,%l3,%l3
	sll	%l0,30,%i5
	xor	%l2,%i3,%i4
	srl	%l0,2,%l0
	add	%i4,%l3,%l3
	or	%i5,%l0,%l0
	add	%g1,%l3,%l3
	sllx	%o5,32,%g4	! Xupdate(62)
	xor	%o0,%o7,%o7
	srlx	%g1,32,%i4
	xor	%o3,%o7,%o7
	sll	%l3,5,%i3		!! 62
	or	%i4,%g4,%g4
	add	%g5,%l2,%l2		!!
	xor	%g4,%o7,%o7
	srlx	%o7,31,%g4
	add	%o7,%o7,%o7
	and	%g4,%g2,%g4
	andn	%o7,%g2,%o7
	srl	%l3,27,%i4		!!
	or	%g4,%o7,%o7
	srlx	%o7,32,%g4
	add	%i3,%l2,%l2		!!
	xor	%l0,%l4,%i3
	add	%i4,%l2,%l2
	sll	%l4,30,%i5
	xor	%l1,%i3,%i4
	srl	%l4,2,%l4
	add	%i4,%l2,%l2
	or	%i5,%l4,%l4
	add	%g4,%l2,%l2
	sll	%l2,5,%i3		!! 63
	add	%g5,%l1,%l1
	srl	%l2,27,%i4
	add	%i3,%l1,%l1		!!
	xor	%l4,%l3,%i3
	add	%i4,%l1,%l1
	sll	%l3,30,%i5
	xor	%l0,%i3,%i4
	srl	%l3,2,%l3
	add	%i4,%l1,%l1
	or	%i5,%l3,%l3
	add	%o7,%l1,%l1
	sllx	%g1,32,%g4	! Xupdate(64)
	xor	%o1,%o0,%o0
	srlx	%o7,32,%i4
	xor	%o4,%o0,%o0
	sll	%l1,5,%i3		!! 64
	or	%i4,%g4,%g4
	add	%g5,%l0,%l0		!!
	xor	%g4,%o0,%o0
	srlx	%o0,31,%g4
	add	%o0,%o0,%o0
	and	%g4,%g2,%g4
	andn	%o0,%g2,%o0
	srl	%l1,27,%i4		!!
	or	%g4,%o0,%o0
	srlx	%o0,32,%g4
	add	%i3,%l0,%l0		!!
	xor	%l3,%l2,%i3
	add	%i4,%l0,%l0
	sll	%l2,30,%i5
	xor	%l4,%i3,%i4
	srl	%l2,2,%l2
	add	%i4,%l0,%l0
	or	%i5,%l2,%l2
	add	%g4,%l0,%l0
	sll	%l0,5,%i3		!! 65
	add	%g5,%l4,%l4
	srl	%l0,27,%i4
	add	%i3,%l4,%l4		!!
	xor	%l2,%l1,%i3
	add	%i4,%l4,%l4
	sll	%l1,30,%i5
	xor	%l3,%i3,%i4
	srl	%l1,2,%l1
	add	%i4,%l4,%l4
	or	%i5,%l1,%l1
	add	%o0,%l4,%l4
	sllx	%o7,32,%g4	! Xupdate(66)
	xor	%o2,%o1,%o1
	srlx	%o0,32,%i4
	xor	%o5,%o1,%o1
	sll	%l4,5,%i3		!! 66
	or	%i4,%g4,%g4
	add	%g5,%l3,%l3		!!
	xor	%g4,%o1,%o1
	srlx	%o1,31,%g4
	add	%o1,%o1,%o1
	and	%g4,%g2,%g4
	andn	%o1,%g2,%o1
	srl	%l4,27,%i4		!!
	or	%g4,%o1,%o1
	srlx	%o1,32,%g4
	add	%i3,%l3,%l3		!!
	xor	%l1,%l0,%i3
	add	%i4,%l3,%l3
	sll	%l0,30,%i5
	xor	%l2,%i3,%i4
	srl	%l0,2,%l0
	add	%i4,%l3,%l3
	or	%i5,%l0,%l0
	add	%g4,%l3,%l3
	sll	%l3,5,%i3		!! 67
	add	%g5,%l2,%l2
	srl	%l3,27,%i4
	add	%i3,%l2,%l2		!!
	xor	%l0,%l4,%i3
	add	%i4,%l2,%l2
	sll	%l4,30,%i5
	xor	%l1,%i3,%i4
	srl	%l4,2,%l4
	add	%i4,%l2,%l2
	or	%i5,%l4,%l4
	add	%o1,%l2,%l2
	sllx	%o0,32,%g4	! Xupdate(68)
	xor	%o3,%o2,%o2
	srlx	%o1,32,%i4
	xor	%g1,%o2,%o2
	sll	%l2,5,%i3		!! 68
	or	%i4,%g4,%g4
	add	%g5,%l1,%l1		!!
	xor	%g4,%o2,%o2
	srlx	%o2,31,%g4
	add	%o2,%o2,%o2
	and	%g4,%g2,%g4
	andn	%o2,%g2,%o2
	srl	%l2,27,%i4		!!
	or	%g4,%o2,%o2
	srlx	%o2,32,%g4
	add	%i3,%l1,%l1		!!
	xor	%l4,%l3,%i3
	add	%i4,%l1,%l1
	sll	%l3,30,%i5
	xor	%l0,%i3,%i4
	srl	%l3,2,%l3
	add	%i4,%l1,%l1
	or	%i5,%l3,%l3
	add	%g4,%l1,%l1
	sll	%l1,5,%i3		!! 69
	add	%g5,%l0,%l0
	srl	%l1,27,%i4
	add	%i3,%l0,%l0		!!
	xor	%l3,%l2,%i3
	add	%i4,%l0,%l0
	sll	%l2,30,%i5
	xor	%l4,%i3,%i4
	srl	%l2,2,%l2
	add	%i4,%l0,%l0
	or	%i5,%l2,%l2
	add	%o2,%l0,%l0
	sllx	%o1,32,%g4	! Xupdate(70)
	xor	%o4,%o3,%o3
	srlx	%o2,32,%i4
	xor	%o7,%o3,%o3
	sll	%l0,5,%i3		!! 70
	or	%i4,%g4,%g4
	add	%g5,%l4,%l4		!!
	xor	%g4,%o3,%o3
	srlx	%o3,31,%g4
	add	%o3,%o3,%o3
	and	%g4,%g2,%g4
	andn	%o3,%g2,%o3
	srl	%l0,27,%i4		!!
	or	%g4,%o3,%o3
	srlx	%o3,32,%g4
	add	%i3,%l4,%l4		!!
	xor	%l2,%l1,%i3
	add	%i4,%l4,%l4
	sll	%l1,30,%i5
	xor	%l3,%i3,%i4
	srl	%l1,2,%l1
	add	%i4,%l4,%l4
	or	%i5,%l1,%l1
	add	%g4,%l4,%l4
	sll	%l4,5,%i3		!! 71
	add	%g5,%l3,%l3
	srl	%l4,27,%i4
	add	%i3,%l3,%l3		!!
	xor	%l1,%l0,%i3
	add	%i4,%l3,%l3
	sll	%l0,30,%i5
	xor	%l2,%i3,%i4
	srl	%l0,2,%l0
	add	%i4,%l3,%l3
	or	%i5,%l0,%l0
	add	%o3,%l3,%l3
	sllx	%o2,32,%g4	! Xupdate(72)
	xor	%o5,%o4,%o4
	srlx	%o3,32,%i4
	xor	%o0,%o4,%o4
	sll	%l3,5,%i3		!! 72
	or	%i4,%g4,%g4
	add	%g5,%l2,%l2		!!
	xor	%g4,%o4,%o4
	srlx	%o4,31,%g4
	add	%o4,%o4,%o4
	and	%g4,%g2,%g4
	andn	%o4,%g2,%o4
	srl	%l3,27,%i4		!!
	or	%g4,%o4,%o4
	srlx	%o4,32,%g4
	add	%i3,%l2,%l2		!!
	xor	%l0,%l4,%i3
	add	%i4,%l2,%l2
	sll	%l4,30,%i5
	xor	%l1,%i3,%i4
	srl	%l4,2,%l4
	add	%i4,%l2,%l2
	or	%i5,%l4,%l4
	add	%g4,%l2,%l2
	sll	%l2,5,%i3		!! 73
	add	%g5,%l1,%l1
	srl	%l2,27,%i4
	add	%i3,%l1,%l1		!!
	xor	%l4,%l3,%i3
	add	%i4,%l1,%l1
	sll	%l3,30,%i5
	xor	%l0,%i3,%i4
	srl	%l3,2,%l3
	add	%i4,%l1,%l1
	or	%i5,%l3,%l3
	add	%o4,%l1,%l1
	sllx	%o3,32,%g4	! Xupdate(74)
	xor	%g1,%o5,%o5
	srlx	%o4,32,%i4
	xor	%o1,%o5,%o5
	sll	%l1,5,%i3		!! 74
	or	%i4,%g4,%g4
	add	%g5,%l0,%l0		!!
	xor	%g4,%o5,%o5
	srlx	%o5,31,%g4
	add	%o5,%o5,%o5
	and	%g4,%g2,%g4
	andn	%o5,%g2,%o5
	srl	%l1,27,%i4		!!
	or	%g4,%o5,%o5
	srlx	%o5,32,%g4
	add	%i3,%l0,%l0		!!
	xor	%l3,%l2,%i3
	add	%i4,%l0,%l0
	sll	%l2,30,%i5
	xor	%l4,%i3,%i4
	srl	%l2,2,%l2
	add	%i4,%l0,%l0
	or	%i5,%l2,%l2
	add	%g4,%l0,%l0
	sll	%l0,5,%i3		!! 75
	add	%g5,%l4,%l4
	srl	%l0,27,%i4
	add	%i3,%l4,%l4		!!
	xor	%l2,%l1,%i3
	add	%i4,%l4,%l4
	sll	%l1,30,%i5
	xor	%l3,%i3,%i4
	srl	%l1,2,%l1
	add	%i4,%l4,%l4
	or	%i5,%l1,%l1
	add	%o5,%l4,%l4
	sllx	%o4,32,%g4	! Xupdate(76)
	xor	%o7,%g1,%g1
	srlx	%o5,32,%i4
	xor	%o2,%g1,%g1
	sll	%l4,5,%i3		!! 76
	or	%i4,%g4,%g4
	add	%g5,%l3,%l3		!!
	xor	%g4,%g1,%g1
	srlx	%g1,31,%g4
	add	%g1,%g1,%g1
	and	%g4,%g2,%g4
	andn	%g1,%g2,%g1
	srl	%l4,27,%i4		!!
	or	%g4,%g1,%g1
	srlx	%g1,32,%g4
	add	%i3,%l3,%l3		!!
	xor	%l1,%l0,%i3
	add	%i4,%l3,%l3
	sll	%l0,30,%i5
	xor	%l2,%i3,%i4
	srl	%l0,2,%l0
	add	%i4,%l3,%l3
	or	%i5,%l0,%l0
	add	%g4,%l3,%l3
	sll	%l3,5,%i3		!! 77
	add	%g5,%l2,%l2
	srl	%l3,27,%i4
	add	%i3,%l2,%l2		!!
	xor	%l0,%l4,%i3
	add	%i4,%l2,%l2
	sll	%l4,30,%i5
	xor	%l1,%i3,%i4
	srl	%l4,2,%l4
	add	%i4,%l2,%l2
	or	%i5,%l4,%l4
	add	%g1,%l2,%l2
	sllx	%o5,32,%g4	! Xupdate(78)
	xor	%o0,%o7,%o7
	srlx	%g1,32,%i4
	xor	%o3,%o7,%o7
	sll	%l2,5,%i3		!! 78
	or	%i4,%g4,%g4
	add	%g5,%l1,%l1		!!
	xor	%g4,%o7,%o7
	srlx	%o7,31,%g4
	add	%o7,%o7,%o7
	and	%g4,%g2,%g4
	andn	%o7,%g2,%o7
	srl	%l2,27,%i4		!!
	or	%g4,%o7,%o7
	srlx	%o7,32,%g4
	add	%i3,%l1,%l1		!!
	xor	%l4,%l3,%i3
	add	%i4,%l1,%l1
	sll	%l3,30,%i5
	xor	%l0,%i3,%i4
	srl	%l3,2,%l3
	add	%i4,%l1,%l1
	or	%i5,%l3,%l3
	add	%g4,%l1,%l1
	sll	%l1,5,%i3		!! 79
	add	%g5,%l0,%l0
	srl	%l1,27,%i4
	add	%i3,%l0,%l0		!!
	xor	%l3,%l2,%i3
	add	%i4,%l0,%l0
	sll	%l2,30,%i5
	xor	%l4,%i3,%i4
	srl	%l2,2,%l2
	add	%i4,%l0,%l0
	or	%i5,%l2,%l2
	add	%o7,%l0,%l0

	ld	[%i0+0],%o0
	ld	[%i0+4],%o1
	ld	[%i0+8],%o2
	ld	[%i0+12],%o3
	add	%i1,64,%i1
	ld	[%i0+16],%o4
	cmp	%i1,%i2

	add	%l0,%o0,%l0
	st	%l0,[%i0+0]
	add	%l1,%o1,%l1
	st	%l1,[%i0+4]
	add	%l2,%o2,%l2
	st	%l2,[%i0+8]
	add	%l3,%o3,%l3
	st	%l3,[%i0+12]
	add	%l4,%o4,%l4
	st	%l4,[%i0+16]

	bne	%xcc,.Lloop
	andn	%i1,7,%i3

	ret
	restore
.type	sha1_block_data_order,#function
.size	sha1_block_data_order,(.-sha1_block_data_order)
.asciz	"SHA1 block transform for SPARCv9, CRYPTOGAMS by <appro@openssl.org>"
.align	4
