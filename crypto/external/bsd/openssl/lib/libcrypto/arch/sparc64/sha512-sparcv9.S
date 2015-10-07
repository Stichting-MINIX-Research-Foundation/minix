.section	".text",#alloc,#execinstr

.align	64
K256:
.type	K256,#object
	.long	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5
	.long	0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5
	.long	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3
	.long	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174
	.long	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc
	.long	0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da
	.long	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7
	.long	0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967
	.long	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13
	.long	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85
	.long	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3
	.long	0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070
	.long	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5
	.long	0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3
	.long	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208
	.long	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
.size	K256,.-K256
.globl	sha256_block_data_order
sha256_block_data_order:
	save	%sp,-112,%sp
	and	%i1,7,%i4
	sllx	%i2,6,%i2
	andn	%i1,7,%i1
	sll	%i4,3,%i4
	add	%i1,%i2,%i2
.Lpic:	call	.+8
	add	%o7,K256-.Lpic,%i3

	ld	[%i0+0],%l0
	ld	[%i0+4],%l1
	ld	[%i0+8],%l2
	ld	[%i0+12],%l3
	ld	[%i0+16],%l4
	ld	[%i0+20],%l5
	ld	[%i0+24],%l6
	ld	[%i0+28],%l7

.Lloop:
	ldx	[%i1+0],%o0
	ldx	[%i1+16],%o2
	ldx	[%i1+32],%o4
	ldx	[%i1+48],%g1
	ldx	[%i1+8],%o1
	ldx	[%i1+24],%o3
	subcc	%g0,%i4,%i5 ! should be 64-%i4, but -%i4 works too
	ldx	[%i1+40],%o5
	bz,pt	%icc,.Laligned
	ldx	[%i1+56],%o7

	sllx	%o0,%i4,%o0
	ldx	[%i1+64],%g2
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
	srlx	%g2,%i5,%g2
	or	%g2,%o7,%o7
.Laligned:
	srlx	%o0,32,%g2
	add	%l7,%g2,%g2
	srl	%l4,6,%l7	!! 0
	xor	%l5,%l6,%g5
	sll	%l4,7,%g4
	and	%l4,%g5,%g5
	srl	%l4,11,%g3
	xor	%g4,%l7,%l7
	sll	%l4,21,%g4
	xor	%g3,%l7,%l7
	srl	%l4,25,%g3
	xor	%g4,%l7,%l7
	sll	%l4,26,%g4
	xor	%g3,%l7,%l7
	xor	%l6,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%l7,%g3		! Sigma1(e)

	srl	%l0,2,%l7
	add	%g5,%g2,%g2
	ld	[%i3+0],%g5	! K[0]
	sll	%l0,10,%g4
	add	%g3,%g2,%g2
	srl	%l0,13,%g3
	xor	%g4,%l7,%l7
	sll	%l0,19,%g4
	xor	%g3,%l7,%l7
	srl	%l0,22,%g3
	xor	%g4,%l7,%l7	
	sll	%l0,30,%g4
	xor	%g3,%l7,%l7
	xor	%g4,%l7,%l7		! Sigma0(a)

	or	%l0,%l1,%g3
	and	%l0,%l1,%g4
	and	%l2,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[0]
	add	%g4,%l7,%l7

	add	%g2,%l3,%l3
	add	%g2,%l7,%l7
	add	%o0,%l6,%g2
	srl	%l3,6,%l6	!! 1
	xor	%l4,%l5,%g5
	sll	%l3,7,%g4
	and	%l3,%g5,%g5
	srl	%l3,11,%g3
	xor	%g4,%l6,%l6
	sll	%l3,21,%g4
	xor	%g3,%l6,%l6
	srl	%l3,25,%g3
	xor	%g4,%l6,%l6
	sll	%l3,26,%g4
	xor	%g3,%l6,%l6
	xor	%l5,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%l6,%g3		! Sigma1(e)

	srl	%l7,2,%l6
	add	%g5,%g2,%g2
	ld	[%i3+4],%g5	! K[1]
	sll	%l7,10,%g4
	add	%g3,%g2,%g2
	srl	%l7,13,%g3
	xor	%g4,%l6,%l6
	sll	%l7,19,%g4
	xor	%g3,%l6,%l6
	srl	%l7,22,%g3
	xor	%g4,%l6,%l6	
	sll	%l7,30,%g4
	xor	%g3,%l6,%l6
	xor	%g4,%l6,%l6		! Sigma0(a)

	or	%l7,%l0,%g3
	and	%l7,%l0,%g4
	and	%l1,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[1]
	add	%g4,%l6,%l6

	add	%g2,%l2,%l2
	add	%g2,%l6,%l6
	srlx	%o1,32,%g2
	add	%l5,%g2,%g2
	srl	%l2,6,%l5	!! 2
	xor	%l3,%l4,%g5
	sll	%l2,7,%g4
	and	%l2,%g5,%g5
	srl	%l2,11,%g3
	xor	%g4,%l5,%l5
	sll	%l2,21,%g4
	xor	%g3,%l5,%l5
	srl	%l2,25,%g3
	xor	%g4,%l5,%l5
	sll	%l2,26,%g4
	xor	%g3,%l5,%l5
	xor	%l4,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%l5,%g3		! Sigma1(e)

	srl	%l6,2,%l5
	add	%g5,%g2,%g2
	ld	[%i3+8],%g5	! K[2]
	sll	%l6,10,%g4
	add	%g3,%g2,%g2
	srl	%l6,13,%g3
	xor	%g4,%l5,%l5
	sll	%l6,19,%g4
	xor	%g3,%l5,%l5
	srl	%l6,22,%g3
	xor	%g4,%l5,%l5	
	sll	%l6,30,%g4
	xor	%g3,%l5,%l5
	xor	%g4,%l5,%l5		! Sigma0(a)

	or	%l6,%l7,%g3
	and	%l6,%l7,%g4
	and	%l0,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[2]
	add	%g4,%l5,%l5

	add	%g2,%l1,%l1
	add	%g2,%l5,%l5
	add	%o1,%l4,%g2
	srl	%l1,6,%l4	!! 3
	xor	%l2,%l3,%g5
	sll	%l1,7,%g4
	and	%l1,%g5,%g5
	srl	%l1,11,%g3
	xor	%g4,%l4,%l4
	sll	%l1,21,%g4
	xor	%g3,%l4,%l4
	srl	%l1,25,%g3
	xor	%g4,%l4,%l4
	sll	%l1,26,%g4
	xor	%g3,%l4,%l4
	xor	%l3,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%l4,%g3		! Sigma1(e)

	srl	%l5,2,%l4
	add	%g5,%g2,%g2
	ld	[%i3+12],%g5	! K[3]
	sll	%l5,10,%g4
	add	%g3,%g2,%g2
	srl	%l5,13,%g3
	xor	%g4,%l4,%l4
	sll	%l5,19,%g4
	xor	%g3,%l4,%l4
	srl	%l5,22,%g3
	xor	%g4,%l4,%l4	
	sll	%l5,30,%g4
	xor	%g3,%l4,%l4
	xor	%g4,%l4,%l4		! Sigma0(a)

	or	%l5,%l6,%g3
	and	%l5,%l6,%g4
	and	%l7,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[3]
	add	%g4,%l4,%l4

	add	%g2,%l0,%l0
	add	%g2,%l4,%l4
	srlx	%o2,32,%g2
	add	%l3,%g2,%g2
	srl	%l0,6,%l3	!! 4
	xor	%l1,%l2,%g5
	sll	%l0,7,%g4
	and	%l0,%g5,%g5
	srl	%l0,11,%g3
	xor	%g4,%l3,%l3
	sll	%l0,21,%g4
	xor	%g3,%l3,%l3
	srl	%l0,25,%g3
	xor	%g4,%l3,%l3
	sll	%l0,26,%g4
	xor	%g3,%l3,%l3
	xor	%l2,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%l3,%g3		! Sigma1(e)

	srl	%l4,2,%l3
	add	%g5,%g2,%g2
	ld	[%i3+16],%g5	! K[4]
	sll	%l4,10,%g4
	add	%g3,%g2,%g2
	srl	%l4,13,%g3
	xor	%g4,%l3,%l3
	sll	%l4,19,%g4
	xor	%g3,%l3,%l3
	srl	%l4,22,%g3
	xor	%g4,%l3,%l3	
	sll	%l4,30,%g4
	xor	%g3,%l3,%l3
	xor	%g4,%l3,%l3		! Sigma0(a)

	or	%l4,%l5,%g3
	and	%l4,%l5,%g4
	and	%l6,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[4]
	add	%g4,%l3,%l3

	add	%g2,%l7,%l7
	add	%g2,%l3,%l3
	add	%o2,%l2,%g2
	srl	%l7,6,%l2	!! 5
	xor	%l0,%l1,%g5
	sll	%l7,7,%g4
	and	%l7,%g5,%g5
	srl	%l7,11,%g3
	xor	%g4,%l2,%l2
	sll	%l7,21,%g4
	xor	%g3,%l2,%l2
	srl	%l7,25,%g3
	xor	%g4,%l2,%l2
	sll	%l7,26,%g4
	xor	%g3,%l2,%l2
	xor	%l1,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%l2,%g3		! Sigma1(e)

	srl	%l3,2,%l2
	add	%g5,%g2,%g2
	ld	[%i3+20],%g5	! K[5]
	sll	%l3,10,%g4
	add	%g3,%g2,%g2
	srl	%l3,13,%g3
	xor	%g4,%l2,%l2
	sll	%l3,19,%g4
	xor	%g3,%l2,%l2
	srl	%l3,22,%g3
	xor	%g4,%l2,%l2	
	sll	%l3,30,%g4
	xor	%g3,%l2,%l2
	xor	%g4,%l2,%l2		! Sigma0(a)

	or	%l3,%l4,%g3
	and	%l3,%l4,%g4
	and	%l5,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[5]
	add	%g4,%l2,%l2

	add	%g2,%l6,%l6
	add	%g2,%l2,%l2
	srlx	%o3,32,%g2
	add	%l1,%g2,%g2
	srl	%l6,6,%l1	!! 6
	xor	%l7,%l0,%g5
	sll	%l6,7,%g4
	and	%l6,%g5,%g5
	srl	%l6,11,%g3
	xor	%g4,%l1,%l1
	sll	%l6,21,%g4
	xor	%g3,%l1,%l1
	srl	%l6,25,%g3
	xor	%g4,%l1,%l1
	sll	%l6,26,%g4
	xor	%g3,%l1,%l1
	xor	%l0,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%l1,%g3		! Sigma1(e)

	srl	%l2,2,%l1
	add	%g5,%g2,%g2
	ld	[%i3+24],%g5	! K[6]
	sll	%l2,10,%g4
	add	%g3,%g2,%g2
	srl	%l2,13,%g3
	xor	%g4,%l1,%l1
	sll	%l2,19,%g4
	xor	%g3,%l1,%l1
	srl	%l2,22,%g3
	xor	%g4,%l1,%l1	
	sll	%l2,30,%g4
	xor	%g3,%l1,%l1
	xor	%g4,%l1,%l1		! Sigma0(a)

	or	%l2,%l3,%g3
	and	%l2,%l3,%g4
	and	%l4,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[6]
	add	%g4,%l1,%l1

	add	%g2,%l5,%l5
	add	%g2,%l1,%l1
	add	%o3,%l0,%g2
	srl	%l5,6,%l0	!! 7
	xor	%l6,%l7,%g5
	sll	%l5,7,%g4
	and	%l5,%g5,%g5
	srl	%l5,11,%g3
	xor	%g4,%l0,%l0
	sll	%l5,21,%g4
	xor	%g3,%l0,%l0
	srl	%l5,25,%g3
	xor	%g4,%l0,%l0
	sll	%l5,26,%g4
	xor	%g3,%l0,%l0
	xor	%l7,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%l0,%g3		! Sigma1(e)

	srl	%l1,2,%l0
	add	%g5,%g2,%g2
	ld	[%i3+28],%g5	! K[7]
	sll	%l1,10,%g4
	add	%g3,%g2,%g2
	srl	%l1,13,%g3
	xor	%g4,%l0,%l0
	sll	%l1,19,%g4
	xor	%g3,%l0,%l0
	srl	%l1,22,%g3
	xor	%g4,%l0,%l0	
	sll	%l1,30,%g4
	xor	%g3,%l0,%l0
	xor	%g4,%l0,%l0		! Sigma0(a)

	or	%l1,%l2,%g3
	and	%l1,%l2,%g4
	and	%l3,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[7]
	add	%g4,%l0,%l0

	add	%g2,%l4,%l4
	add	%g2,%l0,%l0
	srlx	%o4,32,%g2
	add	%l7,%g2,%g2
	srl	%l4,6,%l7	!! 8
	xor	%l5,%l6,%g5
	sll	%l4,7,%g4
	and	%l4,%g5,%g5
	srl	%l4,11,%g3
	xor	%g4,%l7,%l7
	sll	%l4,21,%g4
	xor	%g3,%l7,%l7
	srl	%l4,25,%g3
	xor	%g4,%l7,%l7
	sll	%l4,26,%g4
	xor	%g3,%l7,%l7
	xor	%l6,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%l7,%g3		! Sigma1(e)

	srl	%l0,2,%l7
	add	%g5,%g2,%g2
	ld	[%i3+32],%g5	! K[8]
	sll	%l0,10,%g4
	add	%g3,%g2,%g2
	srl	%l0,13,%g3
	xor	%g4,%l7,%l7
	sll	%l0,19,%g4
	xor	%g3,%l7,%l7
	srl	%l0,22,%g3
	xor	%g4,%l7,%l7	
	sll	%l0,30,%g4
	xor	%g3,%l7,%l7
	xor	%g4,%l7,%l7		! Sigma0(a)

	or	%l0,%l1,%g3
	and	%l0,%l1,%g4
	and	%l2,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[8]
	add	%g4,%l7,%l7

	add	%g2,%l3,%l3
	add	%g2,%l7,%l7
	add	%o4,%l6,%g2
	srl	%l3,6,%l6	!! 9
	xor	%l4,%l5,%g5
	sll	%l3,7,%g4
	and	%l3,%g5,%g5
	srl	%l3,11,%g3
	xor	%g4,%l6,%l6
	sll	%l3,21,%g4
	xor	%g3,%l6,%l6
	srl	%l3,25,%g3
	xor	%g4,%l6,%l6
	sll	%l3,26,%g4
	xor	%g3,%l6,%l6
	xor	%l5,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%l6,%g3		! Sigma1(e)

	srl	%l7,2,%l6
	add	%g5,%g2,%g2
	ld	[%i3+36],%g5	! K[9]
	sll	%l7,10,%g4
	add	%g3,%g2,%g2
	srl	%l7,13,%g3
	xor	%g4,%l6,%l6
	sll	%l7,19,%g4
	xor	%g3,%l6,%l6
	srl	%l7,22,%g3
	xor	%g4,%l6,%l6	
	sll	%l7,30,%g4
	xor	%g3,%l6,%l6
	xor	%g4,%l6,%l6		! Sigma0(a)

	or	%l7,%l0,%g3
	and	%l7,%l0,%g4
	and	%l1,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[9]
	add	%g4,%l6,%l6

	add	%g2,%l2,%l2
	add	%g2,%l6,%l6
	srlx	%o5,32,%g2
	add	%l5,%g2,%g2
	srl	%l2,6,%l5	!! 10
	xor	%l3,%l4,%g5
	sll	%l2,7,%g4
	and	%l2,%g5,%g5
	srl	%l2,11,%g3
	xor	%g4,%l5,%l5
	sll	%l2,21,%g4
	xor	%g3,%l5,%l5
	srl	%l2,25,%g3
	xor	%g4,%l5,%l5
	sll	%l2,26,%g4
	xor	%g3,%l5,%l5
	xor	%l4,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%l5,%g3		! Sigma1(e)

	srl	%l6,2,%l5
	add	%g5,%g2,%g2
	ld	[%i3+40],%g5	! K[10]
	sll	%l6,10,%g4
	add	%g3,%g2,%g2
	srl	%l6,13,%g3
	xor	%g4,%l5,%l5
	sll	%l6,19,%g4
	xor	%g3,%l5,%l5
	srl	%l6,22,%g3
	xor	%g4,%l5,%l5	
	sll	%l6,30,%g4
	xor	%g3,%l5,%l5
	xor	%g4,%l5,%l5		! Sigma0(a)

	or	%l6,%l7,%g3
	and	%l6,%l7,%g4
	and	%l0,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[10]
	add	%g4,%l5,%l5

	add	%g2,%l1,%l1
	add	%g2,%l5,%l5
	add	%o5,%l4,%g2
	srl	%l1,6,%l4	!! 11
	xor	%l2,%l3,%g5
	sll	%l1,7,%g4
	and	%l1,%g5,%g5
	srl	%l1,11,%g3
	xor	%g4,%l4,%l4
	sll	%l1,21,%g4
	xor	%g3,%l4,%l4
	srl	%l1,25,%g3
	xor	%g4,%l4,%l4
	sll	%l1,26,%g4
	xor	%g3,%l4,%l4
	xor	%l3,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%l4,%g3		! Sigma1(e)

	srl	%l5,2,%l4
	add	%g5,%g2,%g2
	ld	[%i3+44],%g5	! K[11]
	sll	%l5,10,%g4
	add	%g3,%g2,%g2
	srl	%l5,13,%g3
	xor	%g4,%l4,%l4
	sll	%l5,19,%g4
	xor	%g3,%l4,%l4
	srl	%l5,22,%g3
	xor	%g4,%l4,%l4	
	sll	%l5,30,%g4
	xor	%g3,%l4,%l4
	xor	%g4,%l4,%l4		! Sigma0(a)

	or	%l5,%l6,%g3
	and	%l5,%l6,%g4
	and	%l7,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[11]
	add	%g4,%l4,%l4

	add	%g2,%l0,%l0
	add	%g2,%l4,%l4
	srlx	%g1,32,%g2
	add	%l3,%g2,%g2
	srl	%l0,6,%l3	!! 12
	xor	%l1,%l2,%g5
	sll	%l0,7,%g4
	and	%l0,%g5,%g5
	srl	%l0,11,%g3
	xor	%g4,%l3,%l3
	sll	%l0,21,%g4
	xor	%g3,%l3,%l3
	srl	%l0,25,%g3
	xor	%g4,%l3,%l3
	sll	%l0,26,%g4
	xor	%g3,%l3,%l3
	xor	%l2,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%l3,%g3		! Sigma1(e)

	srl	%l4,2,%l3
	add	%g5,%g2,%g2
	ld	[%i3+48],%g5	! K[12]
	sll	%l4,10,%g4
	add	%g3,%g2,%g2
	srl	%l4,13,%g3
	xor	%g4,%l3,%l3
	sll	%l4,19,%g4
	xor	%g3,%l3,%l3
	srl	%l4,22,%g3
	xor	%g4,%l3,%l3	
	sll	%l4,30,%g4
	xor	%g3,%l3,%l3
	xor	%g4,%l3,%l3		! Sigma0(a)

	or	%l4,%l5,%g3
	and	%l4,%l5,%g4
	and	%l6,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[12]
	add	%g4,%l3,%l3

	add	%g2,%l7,%l7
	add	%g2,%l3,%l3
	add	%g1,%l2,%g2
	srl	%l7,6,%l2	!! 13
	xor	%l0,%l1,%g5
	sll	%l7,7,%g4
	and	%l7,%g5,%g5
	srl	%l7,11,%g3
	xor	%g4,%l2,%l2
	sll	%l7,21,%g4
	xor	%g3,%l2,%l2
	srl	%l7,25,%g3
	xor	%g4,%l2,%l2
	sll	%l7,26,%g4
	xor	%g3,%l2,%l2
	xor	%l1,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%l2,%g3		! Sigma1(e)

	srl	%l3,2,%l2
	add	%g5,%g2,%g2
	ld	[%i3+52],%g5	! K[13]
	sll	%l3,10,%g4
	add	%g3,%g2,%g2
	srl	%l3,13,%g3
	xor	%g4,%l2,%l2
	sll	%l3,19,%g4
	xor	%g3,%l2,%l2
	srl	%l3,22,%g3
	xor	%g4,%l2,%l2	
	sll	%l3,30,%g4
	xor	%g3,%l2,%l2
	xor	%g4,%l2,%l2		! Sigma0(a)

	or	%l3,%l4,%g3
	and	%l3,%l4,%g4
	and	%l5,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[13]
	add	%g4,%l2,%l2

	add	%g2,%l6,%l6
	add	%g2,%l2,%l2
	srlx	%o7,32,%g2
	add	%l1,%g2,%g2
	srl	%l6,6,%l1	!! 14
	xor	%l7,%l0,%g5
	sll	%l6,7,%g4
	and	%l6,%g5,%g5
	srl	%l6,11,%g3
	xor	%g4,%l1,%l1
	sll	%l6,21,%g4
	xor	%g3,%l1,%l1
	srl	%l6,25,%g3
	xor	%g4,%l1,%l1
	sll	%l6,26,%g4
	xor	%g3,%l1,%l1
	xor	%l0,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%l1,%g3		! Sigma1(e)

	srl	%l2,2,%l1
	add	%g5,%g2,%g2
	ld	[%i3+56],%g5	! K[14]
	sll	%l2,10,%g4
	add	%g3,%g2,%g2
	srl	%l2,13,%g3
	xor	%g4,%l1,%l1
	sll	%l2,19,%g4
	xor	%g3,%l1,%l1
	srl	%l2,22,%g3
	xor	%g4,%l1,%l1	
	sll	%l2,30,%g4
	xor	%g3,%l1,%l1
	xor	%g4,%l1,%l1		! Sigma0(a)

	or	%l2,%l3,%g3
	and	%l2,%l3,%g4
	and	%l4,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[14]
	add	%g4,%l1,%l1

	add	%g2,%l5,%l5
	add	%g2,%l1,%l1
	add	%o7,%l0,%g2
	srl	%l5,6,%l0	!! 15
	xor	%l6,%l7,%g5
	sll	%l5,7,%g4
	and	%l5,%g5,%g5
	srl	%l5,11,%g3
	xor	%g4,%l0,%l0
	sll	%l5,21,%g4
	xor	%g3,%l0,%l0
	srl	%l5,25,%g3
	xor	%g4,%l0,%l0
	sll	%l5,26,%g4
	xor	%g3,%l0,%l0
	xor	%l7,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%l0,%g3		! Sigma1(e)

	srl	%l1,2,%l0
	add	%g5,%g2,%g2
	ld	[%i3+60],%g5	! K[15]
	sll	%l1,10,%g4
	add	%g3,%g2,%g2
	srl	%l1,13,%g3
	xor	%g4,%l0,%l0
	sll	%l1,19,%g4
	xor	%g3,%l0,%l0
	srl	%l1,22,%g3
	xor	%g4,%l0,%l0	
	sll	%l1,30,%g4
	xor	%g3,%l0,%l0
	xor	%g4,%l0,%l0		! Sigma0(a)

	or	%l1,%l2,%g3
	and	%l1,%l2,%g4
	and	%l3,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[15]
	add	%g4,%l0,%l0

	add	%g2,%l4,%l4
	add	%g2,%l0,%l0
.L16_xx:
	srl	%o0,3,%g2		!! Xupdate(16)
	sll	%o0,14,%g4
	srl	%o0,7,%g3
	xor	%g4,%g2,%g2
	sll	%g4,11,%g4
	xor	%g3,%g2,%g2
	srl	%o0,18,%g3
	xor	%g4,%g2,%g2
	srlx	%o7,32,%i5
	srl	%i5,10,%g5
	xor	%g3,%g2,%g2			! T1=sigma0(X[i+1])
	sll	%i5,13,%g4
	srl	%i5,17,%g3
	xor	%g4,%g5,%g5
	sll	%g4,2,%g4
	xor	%g3,%g5,%g5
	srl	%i5,19,%g3
	xor	%g4,%g5,%g5
	srlx	%o0,32,%g4		! X[i]
	xor	%g3,%g5,%g5		! sigma1(X[i+14])
	add	%o4,%g2,%g2			! +=X[i+9]
	add	%g5,%g4,%g4
	srl	%o0,0,%o0
	add	%g4,%g2,%g2

	sllx	%g2,32,%g3
	or	%g3,%o0,%o0
	add	%l7,%g2,%g2
	srl	%l4,6,%l7	!! 16
	xor	%l5,%l6,%g5
	sll	%l4,7,%g4
	and	%l4,%g5,%g5
	srl	%l4,11,%g3
	xor	%g4,%l7,%l7
	sll	%l4,21,%g4
	xor	%g3,%l7,%l7
	srl	%l4,25,%g3
	xor	%g4,%l7,%l7
	sll	%l4,26,%g4
	xor	%g3,%l7,%l7
	xor	%l6,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%l7,%g3		! Sigma1(e)

	srl	%l0,2,%l7
	add	%g5,%g2,%g2
	ld	[%i3+64],%g5	! K[16]
	sll	%l0,10,%g4
	add	%g3,%g2,%g2
	srl	%l0,13,%g3
	xor	%g4,%l7,%l7
	sll	%l0,19,%g4
	xor	%g3,%l7,%l7
	srl	%l0,22,%g3
	xor	%g4,%l7,%l7	
	sll	%l0,30,%g4
	xor	%g3,%l7,%l7
	xor	%g4,%l7,%l7		! Sigma0(a)

	or	%l0,%l1,%g3
	and	%l0,%l1,%g4
	and	%l2,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[16]
	add	%g4,%l7,%l7

	add	%g2,%l3,%l3
	add	%g2,%l7,%l7
	srlx	%o1,32,%i5
	srl	%i5,3,%g2		!! Xupdate(17)
	sll	%i5,14,%g4
	srl	%i5,7,%g3
	xor	%g4,%g2,%g2
	sll	%g4,11,%g4
	xor	%g3,%g2,%g2
	srl	%i5,18,%g3
	xor	%g4,%g2,%g2
	srl	%o7,10,%g5
	xor	%g3,%g2,%g2			! T1=sigma0(X[i+1])
	sll	%o7,13,%g4
	srl	%o7,17,%g3
	xor	%g4,%g5,%g5
	sll	%g4,2,%g4
	xor	%g3,%g5,%g5
	srl	%o7,19,%g3
	xor	%g4,%g5,%g5
	srlx	%o5,32,%g4	! X[i+9]
	xor	%g3,%g5,%g5		! sigma1(X[i+14])
	srl	%o0,0,%g3
	add	%g5,%g4,%g4
	add	%o0,%g2,%g2			! +=X[i]
	xor	%g3,%o0,%o0
	add	%g4,%g2,%g2

	srl	%g2,0,%g2
	or	%g2,%o0,%o0
	add	%l6,%g2,%g2
	srl	%l3,6,%l6	!! 17
	xor	%l4,%l5,%g5
	sll	%l3,7,%g4
	and	%l3,%g5,%g5
	srl	%l3,11,%g3
	xor	%g4,%l6,%l6
	sll	%l3,21,%g4
	xor	%g3,%l6,%l6
	srl	%l3,25,%g3
	xor	%g4,%l6,%l6
	sll	%l3,26,%g4
	xor	%g3,%l6,%l6
	xor	%l5,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%l6,%g3		! Sigma1(e)

	srl	%l7,2,%l6
	add	%g5,%g2,%g2
	ld	[%i3+68],%g5	! K[17]
	sll	%l7,10,%g4
	add	%g3,%g2,%g2
	srl	%l7,13,%g3
	xor	%g4,%l6,%l6
	sll	%l7,19,%g4
	xor	%g3,%l6,%l6
	srl	%l7,22,%g3
	xor	%g4,%l6,%l6	
	sll	%l7,30,%g4
	xor	%g3,%l6,%l6
	xor	%g4,%l6,%l6		! Sigma0(a)

	or	%l7,%l0,%g3
	and	%l7,%l0,%g4
	and	%l1,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[17]
	add	%g4,%l6,%l6

	add	%g2,%l2,%l2
	add	%g2,%l6,%l6
	srl	%o1,3,%g2		!! Xupdate(18)
	sll	%o1,14,%g4
	srl	%o1,7,%g3
	xor	%g4,%g2,%g2
	sll	%g4,11,%g4
	xor	%g3,%g2,%g2
	srl	%o1,18,%g3
	xor	%g4,%g2,%g2
	srlx	%o0,32,%i5
	srl	%i5,10,%g5
	xor	%g3,%g2,%g2			! T1=sigma0(X[i+1])
	sll	%i5,13,%g4
	srl	%i5,17,%g3
	xor	%g4,%g5,%g5
	sll	%g4,2,%g4
	xor	%g3,%g5,%g5
	srl	%i5,19,%g3
	xor	%g4,%g5,%g5
	srlx	%o1,32,%g4		! X[i]
	xor	%g3,%g5,%g5		! sigma1(X[i+14])
	add	%o5,%g2,%g2			! +=X[i+9]
	add	%g5,%g4,%g4
	srl	%o1,0,%o1
	add	%g4,%g2,%g2

	sllx	%g2,32,%g3
	or	%g3,%o1,%o1
	add	%l5,%g2,%g2
	srl	%l2,6,%l5	!! 18
	xor	%l3,%l4,%g5
	sll	%l2,7,%g4
	and	%l2,%g5,%g5
	srl	%l2,11,%g3
	xor	%g4,%l5,%l5
	sll	%l2,21,%g4
	xor	%g3,%l5,%l5
	srl	%l2,25,%g3
	xor	%g4,%l5,%l5
	sll	%l2,26,%g4
	xor	%g3,%l5,%l5
	xor	%l4,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%l5,%g3		! Sigma1(e)

	srl	%l6,2,%l5
	add	%g5,%g2,%g2
	ld	[%i3+72],%g5	! K[18]
	sll	%l6,10,%g4
	add	%g3,%g2,%g2
	srl	%l6,13,%g3
	xor	%g4,%l5,%l5
	sll	%l6,19,%g4
	xor	%g3,%l5,%l5
	srl	%l6,22,%g3
	xor	%g4,%l5,%l5	
	sll	%l6,30,%g4
	xor	%g3,%l5,%l5
	xor	%g4,%l5,%l5		! Sigma0(a)

	or	%l6,%l7,%g3
	and	%l6,%l7,%g4
	and	%l0,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[18]
	add	%g4,%l5,%l5

	add	%g2,%l1,%l1
	add	%g2,%l5,%l5
	srlx	%o2,32,%i5
	srl	%i5,3,%g2		!! Xupdate(19)
	sll	%i5,14,%g4
	srl	%i5,7,%g3
	xor	%g4,%g2,%g2
	sll	%g4,11,%g4
	xor	%g3,%g2,%g2
	srl	%i5,18,%g3
	xor	%g4,%g2,%g2
	srl	%o0,10,%g5
	xor	%g3,%g2,%g2			! T1=sigma0(X[i+1])
	sll	%o0,13,%g4
	srl	%o0,17,%g3
	xor	%g4,%g5,%g5
	sll	%g4,2,%g4
	xor	%g3,%g5,%g5
	srl	%o0,19,%g3
	xor	%g4,%g5,%g5
	srlx	%g1,32,%g4	! X[i+9]
	xor	%g3,%g5,%g5		! sigma1(X[i+14])
	srl	%o1,0,%g3
	add	%g5,%g4,%g4
	add	%o1,%g2,%g2			! +=X[i]
	xor	%g3,%o1,%o1
	add	%g4,%g2,%g2

	srl	%g2,0,%g2
	or	%g2,%o1,%o1
	add	%l4,%g2,%g2
	srl	%l1,6,%l4	!! 19
	xor	%l2,%l3,%g5
	sll	%l1,7,%g4
	and	%l1,%g5,%g5
	srl	%l1,11,%g3
	xor	%g4,%l4,%l4
	sll	%l1,21,%g4
	xor	%g3,%l4,%l4
	srl	%l1,25,%g3
	xor	%g4,%l4,%l4
	sll	%l1,26,%g4
	xor	%g3,%l4,%l4
	xor	%l3,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%l4,%g3		! Sigma1(e)

	srl	%l5,2,%l4
	add	%g5,%g2,%g2
	ld	[%i3+76],%g5	! K[19]
	sll	%l5,10,%g4
	add	%g3,%g2,%g2
	srl	%l5,13,%g3
	xor	%g4,%l4,%l4
	sll	%l5,19,%g4
	xor	%g3,%l4,%l4
	srl	%l5,22,%g3
	xor	%g4,%l4,%l4	
	sll	%l5,30,%g4
	xor	%g3,%l4,%l4
	xor	%g4,%l4,%l4		! Sigma0(a)

	or	%l5,%l6,%g3
	and	%l5,%l6,%g4
	and	%l7,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[19]
	add	%g4,%l4,%l4

	add	%g2,%l0,%l0
	add	%g2,%l4,%l4
	srl	%o2,3,%g2		!! Xupdate(20)
	sll	%o2,14,%g4
	srl	%o2,7,%g3
	xor	%g4,%g2,%g2
	sll	%g4,11,%g4
	xor	%g3,%g2,%g2
	srl	%o2,18,%g3
	xor	%g4,%g2,%g2
	srlx	%o1,32,%i5
	srl	%i5,10,%g5
	xor	%g3,%g2,%g2			! T1=sigma0(X[i+1])
	sll	%i5,13,%g4
	srl	%i5,17,%g3
	xor	%g4,%g5,%g5
	sll	%g4,2,%g4
	xor	%g3,%g5,%g5
	srl	%i5,19,%g3
	xor	%g4,%g5,%g5
	srlx	%o2,32,%g4		! X[i]
	xor	%g3,%g5,%g5		! sigma1(X[i+14])
	add	%g1,%g2,%g2			! +=X[i+9]
	add	%g5,%g4,%g4
	srl	%o2,0,%o2
	add	%g4,%g2,%g2

	sllx	%g2,32,%g3
	or	%g3,%o2,%o2
	add	%l3,%g2,%g2
	srl	%l0,6,%l3	!! 20
	xor	%l1,%l2,%g5
	sll	%l0,7,%g4
	and	%l0,%g5,%g5
	srl	%l0,11,%g3
	xor	%g4,%l3,%l3
	sll	%l0,21,%g4
	xor	%g3,%l3,%l3
	srl	%l0,25,%g3
	xor	%g4,%l3,%l3
	sll	%l0,26,%g4
	xor	%g3,%l3,%l3
	xor	%l2,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%l3,%g3		! Sigma1(e)

	srl	%l4,2,%l3
	add	%g5,%g2,%g2
	ld	[%i3+80],%g5	! K[20]
	sll	%l4,10,%g4
	add	%g3,%g2,%g2
	srl	%l4,13,%g3
	xor	%g4,%l3,%l3
	sll	%l4,19,%g4
	xor	%g3,%l3,%l3
	srl	%l4,22,%g3
	xor	%g4,%l3,%l3	
	sll	%l4,30,%g4
	xor	%g3,%l3,%l3
	xor	%g4,%l3,%l3		! Sigma0(a)

	or	%l4,%l5,%g3
	and	%l4,%l5,%g4
	and	%l6,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[20]
	add	%g4,%l3,%l3

	add	%g2,%l7,%l7
	add	%g2,%l3,%l3
	srlx	%o3,32,%i5
	srl	%i5,3,%g2		!! Xupdate(21)
	sll	%i5,14,%g4
	srl	%i5,7,%g3
	xor	%g4,%g2,%g2
	sll	%g4,11,%g4
	xor	%g3,%g2,%g2
	srl	%i5,18,%g3
	xor	%g4,%g2,%g2
	srl	%o1,10,%g5
	xor	%g3,%g2,%g2			! T1=sigma0(X[i+1])
	sll	%o1,13,%g4
	srl	%o1,17,%g3
	xor	%g4,%g5,%g5
	sll	%g4,2,%g4
	xor	%g3,%g5,%g5
	srl	%o1,19,%g3
	xor	%g4,%g5,%g5
	srlx	%o7,32,%g4	! X[i+9]
	xor	%g3,%g5,%g5		! sigma1(X[i+14])
	srl	%o2,0,%g3
	add	%g5,%g4,%g4
	add	%o2,%g2,%g2			! +=X[i]
	xor	%g3,%o2,%o2
	add	%g4,%g2,%g2

	srl	%g2,0,%g2
	or	%g2,%o2,%o2
	add	%l2,%g2,%g2
	srl	%l7,6,%l2	!! 21
	xor	%l0,%l1,%g5
	sll	%l7,7,%g4
	and	%l7,%g5,%g5
	srl	%l7,11,%g3
	xor	%g4,%l2,%l2
	sll	%l7,21,%g4
	xor	%g3,%l2,%l2
	srl	%l7,25,%g3
	xor	%g4,%l2,%l2
	sll	%l7,26,%g4
	xor	%g3,%l2,%l2
	xor	%l1,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%l2,%g3		! Sigma1(e)

	srl	%l3,2,%l2
	add	%g5,%g2,%g2
	ld	[%i3+84],%g5	! K[21]
	sll	%l3,10,%g4
	add	%g3,%g2,%g2
	srl	%l3,13,%g3
	xor	%g4,%l2,%l2
	sll	%l3,19,%g4
	xor	%g3,%l2,%l2
	srl	%l3,22,%g3
	xor	%g4,%l2,%l2	
	sll	%l3,30,%g4
	xor	%g3,%l2,%l2
	xor	%g4,%l2,%l2		! Sigma0(a)

	or	%l3,%l4,%g3
	and	%l3,%l4,%g4
	and	%l5,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[21]
	add	%g4,%l2,%l2

	add	%g2,%l6,%l6
	add	%g2,%l2,%l2
	srl	%o3,3,%g2		!! Xupdate(22)
	sll	%o3,14,%g4
	srl	%o3,7,%g3
	xor	%g4,%g2,%g2
	sll	%g4,11,%g4
	xor	%g3,%g2,%g2
	srl	%o3,18,%g3
	xor	%g4,%g2,%g2
	srlx	%o2,32,%i5
	srl	%i5,10,%g5
	xor	%g3,%g2,%g2			! T1=sigma0(X[i+1])
	sll	%i5,13,%g4
	srl	%i5,17,%g3
	xor	%g4,%g5,%g5
	sll	%g4,2,%g4
	xor	%g3,%g5,%g5
	srl	%i5,19,%g3
	xor	%g4,%g5,%g5
	srlx	%o3,32,%g4		! X[i]
	xor	%g3,%g5,%g5		! sigma1(X[i+14])
	add	%o7,%g2,%g2			! +=X[i+9]
	add	%g5,%g4,%g4
	srl	%o3,0,%o3
	add	%g4,%g2,%g2

	sllx	%g2,32,%g3
	or	%g3,%o3,%o3
	add	%l1,%g2,%g2
	srl	%l6,6,%l1	!! 22
	xor	%l7,%l0,%g5
	sll	%l6,7,%g4
	and	%l6,%g5,%g5
	srl	%l6,11,%g3
	xor	%g4,%l1,%l1
	sll	%l6,21,%g4
	xor	%g3,%l1,%l1
	srl	%l6,25,%g3
	xor	%g4,%l1,%l1
	sll	%l6,26,%g4
	xor	%g3,%l1,%l1
	xor	%l0,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%l1,%g3		! Sigma1(e)

	srl	%l2,2,%l1
	add	%g5,%g2,%g2
	ld	[%i3+88],%g5	! K[22]
	sll	%l2,10,%g4
	add	%g3,%g2,%g2
	srl	%l2,13,%g3
	xor	%g4,%l1,%l1
	sll	%l2,19,%g4
	xor	%g3,%l1,%l1
	srl	%l2,22,%g3
	xor	%g4,%l1,%l1	
	sll	%l2,30,%g4
	xor	%g3,%l1,%l1
	xor	%g4,%l1,%l1		! Sigma0(a)

	or	%l2,%l3,%g3
	and	%l2,%l3,%g4
	and	%l4,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[22]
	add	%g4,%l1,%l1

	add	%g2,%l5,%l5
	add	%g2,%l1,%l1
	srlx	%o4,32,%i5
	srl	%i5,3,%g2		!! Xupdate(23)
	sll	%i5,14,%g4
	srl	%i5,7,%g3
	xor	%g4,%g2,%g2
	sll	%g4,11,%g4
	xor	%g3,%g2,%g2
	srl	%i5,18,%g3
	xor	%g4,%g2,%g2
	srl	%o2,10,%g5
	xor	%g3,%g2,%g2			! T1=sigma0(X[i+1])
	sll	%o2,13,%g4
	srl	%o2,17,%g3
	xor	%g4,%g5,%g5
	sll	%g4,2,%g4
	xor	%g3,%g5,%g5
	srl	%o2,19,%g3
	xor	%g4,%g5,%g5
	srlx	%o0,32,%g4	! X[i+9]
	xor	%g3,%g5,%g5		! sigma1(X[i+14])
	srl	%o3,0,%g3
	add	%g5,%g4,%g4
	add	%o3,%g2,%g2			! +=X[i]
	xor	%g3,%o3,%o3
	add	%g4,%g2,%g2

	srl	%g2,0,%g2
	or	%g2,%o3,%o3
	add	%l0,%g2,%g2
	srl	%l5,6,%l0	!! 23
	xor	%l6,%l7,%g5
	sll	%l5,7,%g4
	and	%l5,%g5,%g5
	srl	%l5,11,%g3
	xor	%g4,%l0,%l0
	sll	%l5,21,%g4
	xor	%g3,%l0,%l0
	srl	%l5,25,%g3
	xor	%g4,%l0,%l0
	sll	%l5,26,%g4
	xor	%g3,%l0,%l0
	xor	%l7,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%l0,%g3		! Sigma1(e)

	srl	%l1,2,%l0
	add	%g5,%g2,%g2
	ld	[%i3+92],%g5	! K[23]
	sll	%l1,10,%g4
	add	%g3,%g2,%g2
	srl	%l1,13,%g3
	xor	%g4,%l0,%l0
	sll	%l1,19,%g4
	xor	%g3,%l0,%l0
	srl	%l1,22,%g3
	xor	%g4,%l0,%l0	
	sll	%l1,30,%g4
	xor	%g3,%l0,%l0
	xor	%g4,%l0,%l0		! Sigma0(a)

	or	%l1,%l2,%g3
	and	%l1,%l2,%g4
	and	%l3,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[23]
	add	%g4,%l0,%l0

	add	%g2,%l4,%l4
	add	%g2,%l0,%l0
	srl	%o4,3,%g2		!! Xupdate(24)
	sll	%o4,14,%g4
	srl	%o4,7,%g3
	xor	%g4,%g2,%g2
	sll	%g4,11,%g4
	xor	%g3,%g2,%g2
	srl	%o4,18,%g3
	xor	%g4,%g2,%g2
	srlx	%o3,32,%i5
	srl	%i5,10,%g5
	xor	%g3,%g2,%g2			! T1=sigma0(X[i+1])
	sll	%i5,13,%g4
	srl	%i5,17,%g3
	xor	%g4,%g5,%g5
	sll	%g4,2,%g4
	xor	%g3,%g5,%g5
	srl	%i5,19,%g3
	xor	%g4,%g5,%g5
	srlx	%o4,32,%g4		! X[i]
	xor	%g3,%g5,%g5		! sigma1(X[i+14])
	add	%o0,%g2,%g2			! +=X[i+9]
	add	%g5,%g4,%g4
	srl	%o4,0,%o4
	add	%g4,%g2,%g2

	sllx	%g2,32,%g3
	or	%g3,%o4,%o4
	add	%l7,%g2,%g2
	srl	%l4,6,%l7	!! 24
	xor	%l5,%l6,%g5
	sll	%l4,7,%g4
	and	%l4,%g5,%g5
	srl	%l4,11,%g3
	xor	%g4,%l7,%l7
	sll	%l4,21,%g4
	xor	%g3,%l7,%l7
	srl	%l4,25,%g3
	xor	%g4,%l7,%l7
	sll	%l4,26,%g4
	xor	%g3,%l7,%l7
	xor	%l6,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%l7,%g3		! Sigma1(e)

	srl	%l0,2,%l7
	add	%g5,%g2,%g2
	ld	[%i3+96],%g5	! K[24]
	sll	%l0,10,%g4
	add	%g3,%g2,%g2
	srl	%l0,13,%g3
	xor	%g4,%l7,%l7
	sll	%l0,19,%g4
	xor	%g3,%l7,%l7
	srl	%l0,22,%g3
	xor	%g4,%l7,%l7	
	sll	%l0,30,%g4
	xor	%g3,%l7,%l7
	xor	%g4,%l7,%l7		! Sigma0(a)

	or	%l0,%l1,%g3
	and	%l0,%l1,%g4
	and	%l2,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[24]
	add	%g4,%l7,%l7

	add	%g2,%l3,%l3
	add	%g2,%l7,%l7
	srlx	%o5,32,%i5
	srl	%i5,3,%g2		!! Xupdate(25)
	sll	%i5,14,%g4
	srl	%i5,7,%g3
	xor	%g4,%g2,%g2
	sll	%g4,11,%g4
	xor	%g3,%g2,%g2
	srl	%i5,18,%g3
	xor	%g4,%g2,%g2
	srl	%o3,10,%g5
	xor	%g3,%g2,%g2			! T1=sigma0(X[i+1])
	sll	%o3,13,%g4
	srl	%o3,17,%g3
	xor	%g4,%g5,%g5
	sll	%g4,2,%g4
	xor	%g3,%g5,%g5
	srl	%o3,19,%g3
	xor	%g4,%g5,%g5
	srlx	%o1,32,%g4	! X[i+9]
	xor	%g3,%g5,%g5		! sigma1(X[i+14])
	srl	%o4,0,%g3
	add	%g5,%g4,%g4
	add	%o4,%g2,%g2			! +=X[i]
	xor	%g3,%o4,%o4
	add	%g4,%g2,%g2

	srl	%g2,0,%g2
	or	%g2,%o4,%o4
	add	%l6,%g2,%g2
	srl	%l3,6,%l6	!! 25
	xor	%l4,%l5,%g5
	sll	%l3,7,%g4
	and	%l3,%g5,%g5
	srl	%l3,11,%g3
	xor	%g4,%l6,%l6
	sll	%l3,21,%g4
	xor	%g3,%l6,%l6
	srl	%l3,25,%g3
	xor	%g4,%l6,%l6
	sll	%l3,26,%g4
	xor	%g3,%l6,%l6
	xor	%l5,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%l6,%g3		! Sigma1(e)

	srl	%l7,2,%l6
	add	%g5,%g2,%g2
	ld	[%i3+100],%g5	! K[25]
	sll	%l7,10,%g4
	add	%g3,%g2,%g2
	srl	%l7,13,%g3
	xor	%g4,%l6,%l6
	sll	%l7,19,%g4
	xor	%g3,%l6,%l6
	srl	%l7,22,%g3
	xor	%g4,%l6,%l6	
	sll	%l7,30,%g4
	xor	%g3,%l6,%l6
	xor	%g4,%l6,%l6		! Sigma0(a)

	or	%l7,%l0,%g3
	and	%l7,%l0,%g4
	and	%l1,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[25]
	add	%g4,%l6,%l6

	add	%g2,%l2,%l2
	add	%g2,%l6,%l6
	srl	%o5,3,%g2		!! Xupdate(26)
	sll	%o5,14,%g4
	srl	%o5,7,%g3
	xor	%g4,%g2,%g2
	sll	%g4,11,%g4
	xor	%g3,%g2,%g2
	srl	%o5,18,%g3
	xor	%g4,%g2,%g2
	srlx	%o4,32,%i5
	srl	%i5,10,%g5
	xor	%g3,%g2,%g2			! T1=sigma0(X[i+1])
	sll	%i5,13,%g4
	srl	%i5,17,%g3
	xor	%g4,%g5,%g5
	sll	%g4,2,%g4
	xor	%g3,%g5,%g5
	srl	%i5,19,%g3
	xor	%g4,%g5,%g5
	srlx	%o5,32,%g4		! X[i]
	xor	%g3,%g5,%g5		! sigma1(X[i+14])
	add	%o1,%g2,%g2			! +=X[i+9]
	add	%g5,%g4,%g4
	srl	%o5,0,%o5
	add	%g4,%g2,%g2

	sllx	%g2,32,%g3
	or	%g3,%o5,%o5
	add	%l5,%g2,%g2
	srl	%l2,6,%l5	!! 26
	xor	%l3,%l4,%g5
	sll	%l2,7,%g4
	and	%l2,%g5,%g5
	srl	%l2,11,%g3
	xor	%g4,%l5,%l5
	sll	%l2,21,%g4
	xor	%g3,%l5,%l5
	srl	%l2,25,%g3
	xor	%g4,%l5,%l5
	sll	%l2,26,%g4
	xor	%g3,%l5,%l5
	xor	%l4,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%l5,%g3		! Sigma1(e)

	srl	%l6,2,%l5
	add	%g5,%g2,%g2
	ld	[%i3+104],%g5	! K[26]
	sll	%l6,10,%g4
	add	%g3,%g2,%g2
	srl	%l6,13,%g3
	xor	%g4,%l5,%l5
	sll	%l6,19,%g4
	xor	%g3,%l5,%l5
	srl	%l6,22,%g3
	xor	%g4,%l5,%l5	
	sll	%l6,30,%g4
	xor	%g3,%l5,%l5
	xor	%g4,%l5,%l5		! Sigma0(a)

	or	%l6,%l7,%g3
	and	%l6,%l7,%g4
	and	%l0,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[26]
	add	%g4,%l5,%l5

	add	%g2,%l1,%l1
	add	%g2,%l5,%l5
	srlx	%g1,32,%i5
	srl	%i5,3,%g2		!! Xupdate(27)
	sll	%i5,14,%g4
	srl	%i5,7,%g3
	xor	%g4,%g2,%g2
	sll	%g4,11,%g4
	xor	%g3,%g2,%g2
	srl	%i5,18,%g3
	xor	%g4,%g2,%g2
	srl	%o4,10,%g5
	xor	%g3,%g2,%g2			! T1=sigma0(X[i+1])
	sll	%o4,13,%g4
	srl	%o4,17,%g3
	xor	%g4,%g5,%g5
	sll	%g4,2,%g4
	xor	%g3,%g5,%g5
	srl	%o4,19,%g3
	xor	%g4,%g5,%g5
	srlx	%o2,32,%g4	! X[i+9]
	xor	%g3,%g5,%g5		! sigma1(X[i+14])
	srl	%o5,0,%g3
	add	%g5,%g4,%g4
	add	%o5,%g2,%g2			! +=X[i]
	xor	%g3,%o5,%o5
	add	%g4,%g2,%g2

	srl	%g2,0,%g2
	or	%g2,%o5,%o5
	add	%l4,%g2,%g2
	srl	%l1,6,%l4	!! 27
	xor	%l2,%l3,%g5
	sll	%l1,7,%g4
	and	%l1,%g5,%g5
	srl	%l1,11,%g3
	xor	%g4,%l4,%l4
	sll	%l1,21,%g4
	xor	%g3,%l4,%l4
	srl	%l1,25,%g3
	xor	%g4,%l4,%l4
	sll	%l1,26,%g4
	xor	%g3,%l4,%l4
	xor	%l3,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%l4,%g3		! Sigma1(e)

	srl	%l5,2,%l4
	add	%g5,%g2,%g2
	ld	[%i3+108],%g5	! K[27]
	sll	%l5,10,%g4
	add	%g3,%g2,%g2
	srl	%l5,13,%g3
	xor	%g4,%l4,%l4
	sll	%l5,19,%g4
	xor	%g3,%l4,%l4
	srl	%l5,22,%g3
	xor	%g4,%l4,%l4	
	sll	%l5,30,%g4
	xor	%g3,%l4,%l4
	xor	%g4,%l4,%l4		! Sigma0(a)

	or	%l5,%l6,%g3
	and	%l5,%l6,%g4
	and	%l7,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[27]
	add	%g4,%l4,%l4

	add	%g2,%l0,%l0
	add	%g2,%l4,%l4
	srl	%g1,3,%g2		!! Xupdate(28)
	sll	%g1,14,%g4
	srl	%g1,7,%g3
	xor	%g4,%g2,%g2
	sll	%g4,11,%g4
	xor	%g3,%g2,%g2
	srl	%g1,18,%g3
	xor	%g4,%g2,%g2
	srlx	%o5,32,%i5
	srl	%i5,10,%g5
	xor	%g3,%g2,%g2			! T1=sigma0(X[i+1])
	sll	%i5,13,%g4
	srl	%i5,17,%g3
	xor	%g4,%g5,%g5
	sll	%g4,2,%g4
	xor	%g3,%g5,%g5
	srl	%i5,19,%g3
	xor	%g4,%g5,%g5
	srlx	%g1,32,%g4		! X[i]
	xor	%g3,%g5,%g5		! sigma1(X[i+14])
	add	%o2,%g2,%g2			! +=X[i+9]
	add	%g5,%g4,%g4
	srl	%g1,0,%g1
	add	%g4,%g2,%g2

	sllx	%g2,32,%g3
	or	%g3,%g1,%g1
	add	%l3,%g2,%g2
	srl	%l0,6,%l3	!! 28
	xor	%l1,%l2,%g5
	sll	%l0,7,%g4
	and	%l0,%g5,%g5
	srl	%l0,11,%g3
	xor	%g4,%l3,%l3
	sll	%l0,21,%g4
	xor	%g3,%l3,%l3
	srl	%l0,25,%g3
	xor	%g4,%l3,%l3
	sll	%l0,26,%g4
	xor	%g3,%l3,%l3
	xor	%l2,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%l3,%g3		! Sigma1(e)

	srl	%l4,2,%l3
	add	%g5,%g2,%g2
	ld	[%i3+112],%g5	! K[28]
	sll	%l4,10,%g4
	add	%g3,%g2,%g2
	srl	%l4,13,%g3
	xor	%g4,%l3,%l3
	sll	%l4,19,%g4
	xor	%g3,%l3,%l3
	srl	%l4,22,%g3
	xor	%g4,%l3,%l3	
	sll	%l4,30,%g4
	xor	%g3,%l3,%l3
	xor	%g4,%l3,%l3		! Sigma0(a)

	or	%l4,%l5,%g3
	and	%l4,%l5,%g4
	and	%l6,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[28]
	add	%g4,%l3,%l3

	add	%g2,%l7,%l7
	add	%g2,%l3,%l3
	srlx	%o7,32,%i5
	srl	%i5,3,%g2		!! Xupdate(29)
	sll	%i5,14,%g4
	srl	%i5,7,%g3
	xor	%g4,%g2,%g2
	sll	%g4,11,%g4
	xor	%g3,%g2,%g2
	srl	%i5,18,%g3
	xor	%g4,%g2,%g2
	srl	%o5,10,%g5
	xor	%g3,%g2,%g2			! T1=sigma0(X[i+1])
	sll	%o5,13,%g4
	srl	%o5,17,%g3
	xor	%g4,%g5,%g5
	sll	%g4,2,%g4
	xor	%g3,%g5,%g5
	srl	%o5,19,%g3
	xor	%g4,%g5,%g5
	srlx	%o3,32,%g4	! X[i+9]
	xor	%g3,%g5,%g5		! sigma1(X[i+14])
	srl	%g1,0,%g3
	add	%g5,%g4,%g4
	add	%g1,%g2,%g2			! +=X[i]
	xor	%g3,%g1,%g1
	add	%g4,%g2,%g2

	srl	%g2,0,%g2
	or	%g2,%g1,%g1
	add	%l2,%g2,%g2
	srl	%l7,6,%l2	!! 29
	xor	%l0,%l1,%g5
	sll	%l7,7,%g4
	and	%l7,%g5,%g5
	srl	%l7,11,%g3
	xor	%g4,%l2,%l2
	sll	%l7,21,%g4
	xor	%g3,%l2,%l2
	srl	%l7,25,%g3
	xor	%g4,%l2,%l2
	sll	%l7,26,%g4
	xor	%g3,%l2,%l2
	xor	%l1,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%l2,%g3		! Sigma1(e)

	srl	%l3,2,%l2
	add	%g5,%g2,%g2
	ld	[%i3+116],%g5	! K[29]
	sll	%l3,10,%g4
	add	%g3,%g2,%g2
	srl	%l3,13,%g3
	xor	%g4,%l2,%l2
	sll	%l3,19,%g4
	xor	%g3,%l2,%l2
	srl	%l3,22,%g3
	xor	%g4,%l2,%l2	
	sll	%l3,30,%g4
	xor	%g3,%l2,%l2
	xor	%g4,%l2,%l2		! Sigma0(a)

	or	%l3,%l4,%g3
	and	%l3,%l4,%g4
	and	%l5,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[29]
	add	%g4,%l2,%l2

	add	%g2,%l6,%l6
	add	%g2,%l2,%l2
	srl	%o7,3,%g2		!! Xupdate(30)
	sll	%o7,14,%g4
	srl	%o7,7,%g3
	xor	%g4,%g2,%g2
	sll	%g4,11,%g4
	xor	%g3,%g2,%g2
	srl	%o7,18,%g3
	xor	%g4,%g2,%g2
	srlx	%g1,32,%i5
	srl	%i5,10,%g5
	xor	%g3,%g2,%g2			! T1=sigma0(X[i+1])
	sll	%i5,13,%g4
	srl	%i5,17,%g3
	xor	%g4,%g5,%g5
	sll	%g4,2,%g4
	xor	%g3,%g5,%g5
	srl	%i5,19,%g3
	xor	%g4,%g5,%g5
	srlx	%o7,32,%g4		! X[i]
	xor	%g3,%g5,%g5		! sigma1(X[i+14])
	add	%o3,%g2,%g2			! +=X[i+9]
	add	%g5,%g4,%g4
	srl	%o7,0,%o7
	add	%g4,%g2,%g2

	sllx	%g2,32,%g3
	or	%g3,%o7,%o7
	add	%l1,%g2,%g2
	srl	%l6,6,%l1	!! 30
	xor	%l7,%l0,%g5
	sll	%l6,7,%g4
	and	%l6,%g5,%g5
	srl	%l6,11,%g3
	xor	%g4,%l1,%l1
	sll	%l6,21,%g4
	xor	%g3,%l1,%l1
	srl	%l6,25,%g3
	xor	%g4,%l1,%l1
	sll	%l6,26,%g4
	xor	%g3,%l1,%l1
	xor	%l0,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%l1,%g3		! Sigma1(e)

	srl	%l2,2,%l1
	add	%g5,%g2,%g2
	ld	[%i3+120],%g5	! K[30]
	sll	%l2,10,%g4
	add	%g3,%g2,%g2
	srl	%l2,13,%g3
	xor	%g4,%l1,%l1
	sll	%l2,19,%g4
	xor	%g3,%l1,%l1
	srl	%l2,22,%g3
	xor	%g4,%l1,%l1	
	sll	%l2,30,%g4
	xor	%g3,%l1,%l1
	xor	%g4,%l1,%l1		! Sigma0(a)

	or	%l2,%l3,%g3
	and	%l2,%l3,%g4
	and	%l4,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[30]
	add	%g4,%l1,%l1

	add	%g2,%l5,%l5
	add	%g2,%l1,%l1
	srlx	%o0,32,%i5
	srl	%i5,3,%g2		!! Xupdate(31)
	sll	%i5,14,%g4
	srl	%i5,7,%g3
	xor	%g4,%g2,%g2
	sll	%g4,11,%g4
	xor	%g3,%g2,%g2
	srl	%i5,18,%g3
	xor	%g4,%g2,%g2
	srl	%g1,10,%g5
	xor	%g3,%g2,%g2			! T1=sigma0(X[i+1])
	sll	%g1,13,%g4
	srl	%g1,17,%g3
	xor	%g4,%g5,%g5
	sll	%g4,2,%g4
	xor	%g3,%g5,%g5
	srl	%g1,19,%g3
	xor	%g4,%g5,%g5
	srlx	%o4,32,%g4	! X[i+9]
	xor	%g3,%g5,%g5		! sigma1(X[i+14])
	srl	%o7,0,%g3
	add	%g5,%g4,%g4
	add	%o7,%g2,%g2			! +=X[i]
	xor	%g3,%o7,%o7
	add	%g4,%g2,%g2

	srl	%g2,0,%g2
	or	%g2,%o7,%o7
	add	%l0,%g2,%g2
	srl	%l5,6,%l0	!! 31
	xor	%l6,%l7,%g5
	sll	%l5,7,%g4
	and	%l5,%g5,%g5
	srl	%l5,11,%g3
	xor	%g4,%l0,%l0
	sll	%l5,21,%g4
	xor	%g3,%l0,%l0
	srl	%l5,25,%g3
	xor	%g4,%l0,%l0
	sll	%l5,26,%g4
	xor	%g3,%l0,%l0
	xor	%l7,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%l0,%g3		! Sigma1(e)

	srl	%l1,2,%l0
	add	%g5,%g2,%g2
	ld	[%i3+124],%g5	! K[31]
	sll	%l1,10,%g4
	add	%g3,%g2,%g2
	srl	%l1,13,%g3
	xor	%g4,%l0,%l0
	sll	%l1,19,%g4
	xor	%g3,%l0,%l0
	srl	%l1,22,%g3
	xor	%g4,%l0,%l0	
	sll	%l1,30,%g4
	xor	%g3,%l0,%l0
	xor	%g4,%l0,%l0		! Sigma0(a)

	or	%l1,%l2,%g3
	and	%l1,%l2,%g4
	and	%l3,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[31]
	add	%g4,%l0,%l0

	add	%g2,%l4,%l4
	add	%g2,%l0,%l0
	and	%g5,0xfff,%g5
	cmp	%g5,2290
	bne	.L16_xx
	add	%i3,64,%i3	! Ktbl+=16

	ld	[%i0+0],%o0
	ld	[%i0+4],%o1
	ld	[%i0+8],%o2
	ld	[%i0+12],%o3
	ld	[%i0+16],%o4
	ld	[%i0+20],%o5
	ld	[%i0+24],%g1
	ld	[%i0+28],%o7

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
	add	%l5,%o5,%l5
	st	%l5,[%i0+20]
	add	%l6,%g1,%l6
	st	%l6,[%i0+24]
	add	%l7,%o7,%l7
	st	%l7,[%i0+28]
	add	%i1,64,%i1		! advance inp
	cmp	%i1,%i2
	bne	%icc,.Lloop
	sub	%i3,192,%i3	! rewind Ktbl

	ret
	restore
.type	sha256_block_data_order,#function
.size	sha256_block_data_order,(.-sha256_block_data_order)
.asciz	"SHA256 block transform for SPARCv9, CRYPTOGAMS by <appro@openssl.org>"
.align	4
