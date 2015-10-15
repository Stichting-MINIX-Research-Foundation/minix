.section	".text",#alloc,#execinstr

.align	64
K512:
.type	K512,#object
	.long	0x428a2f98,0xd728ae22, 0x71374491,0x23ef65cd
	.long	0xb5c0fbcf,0xec4d3b2f, 0xe9b5dba5,0x8189dbbc
	.long	0x3956c25b,0xf348b538, 0x59f111f1,0xb605d019
	.long	0x923f82a4,0xaf194f9b, 0xab1c5ed5,0xda6d8118
	.long	0xd807aa98,0xa3030242, 0x12835b01,0x45706fbe
	.long	0x243185be,0x4ee4b28c, 0x550c7dc3,0xd5ffb4e2
	.long	0x72be5d74,0xf27b896f, 0x80deb1fe,0x3b1696b1
	.long	0x9bdc06a7,0x25c71235, 0xc19bf174,0xcf692694
	.long	0xe49b69c1,0x9ef14ad2, 0xefbe4786,0x384f25e3
	.long	0x0fc19dc6,0x8b8cd5b5, 0x240ca1cc,0x77ac9c65
	.long	0x2de92c6f,0x592b0275, 0x4a7484aa,0x6ea6e483
	.long	0x5cb0a9dc,0xbd41fbd4, 0x76f988da,0x831153b5
	.long	0x983e5152,0xee66dfab, 0xa831c66d,0x2db43210
	.long	0xb00327c8,0x98fb213f, 0xbf597fc7,0xbeef0ee4
	.long	0xc6e00bf3,0x3da88fc2, 0xd5a79147,0x930aa725
	.long	0x06ca6351,0xe003826f, 0x14292967,0x0a0e6e70
	.long	0x27b70a85,0x46d22ffc, 0x2e1b2138,0x5c26c926
	.long	0x4d2c6dfc,0x5ac42aed, 0x53380d13,0x9d95b3df
	.long	0x650a7354,0x8baf63de, 0x766a0abb,0x3c77b2a8
	.long	0x81c2c92e,0x47edaee6, 0x92722c85,0x1482353b
	.long	0xa2bfe8a1,0x4cf10364, 0xa81a664b,0xbc423001
	.long	0xc24b8b70,0xd0f89791, 0xc76c51a3,0x0654be30
	.long	0xd192e819,0xd6ef5218, 0xd6990624,0x5565a910
	.long	0xf40e3585,0x5771202a, 0x106aa070,0x32bbd1b8
	.long	0x19a4c116,0xb8d2d0c8, 0x1e376c08,0x5141ab53
	.long	0x2748774c,0xdf8eeb99, 0x34b0bcb5,0xe19b48a8
	.long	0x391c0cb3,0xc5c95a63, 0x4ed8aa4a,0xe3418acb
	.long	0x5b9cca4f,0x7763e373, 0x682e6ff3,0xd6b2b8a3
	.long	0x748f82ee,0x5defb2fc, 0x78a5636f,0x43172f60
	.long	0x84c87814,0xa1f0ab72, 0x8cc70208,0x1a6439ec
	.long	0x90befffa,0x23631e28, 0xa4506ceb,0xde82bde9
	.long	0xbef9a3f7,0xb2c67915, 0xc67178f2,0xe372532b
	.long	0xca273ece,0xea26619c, 0xd186b8c7,0x21c0c207
	.long	0xeada7dd6,0xcde0eb1e, 0xf57d4f7f,0xee6ed178
	.long	0x06f067aa,0x72176fba, 0x0a637dc5,0xa2c898a6
	.long	0x113f9804,0xbef90dae, 0x1b710b35,0x131c471b
	.long	0x28db77f5,0x23047d84, 0x32caab7b,0x40c72493
	.long	0x3c9ebe0a,0x15c9bebc, 0x431d67c4,0x9c100d4c
	.long	0x4cc5d4be,0xcb3e42b6, 0x597f299c,0xfc657e2a
	.long	0x5fcb6fab,0x3ad6faec, 0x6c44198c,0x4a475817
.size	K512,.-K512
.globl	sha512_block_data_order
sha512_block_data_order:
	save	%sp,-240,%sp
	and	%i1,3,%i4
	sllx	%i2,7,%i2
	andn	%i1,3,%i1
	sll	%i4,3,%i4
	add	%i1,%i2,%i2
	mov	32,%i5
	sub	%i5,%i4,%i5
.Lpic:	call	.+8
	add	%o7,K512-.Lpic,%i3

	ldx	[%i0+0],%o0
	ldx	[%i0+8],%o1
	ldx	[%i0+16],%o2
	ldx	[%i0+24],%o3
	ldx	[%i0+32],%o4
	ldx	[%i0+40],%o5
	ldx	[%i0+48],%g1
	ldx	[%i0+56],%o7

.Lloop:
	ld	[%i1+0],%l0
	ld	[%i1+4],%l1
	ld	[%i1+8],%l2
	ld	[%i1+12],%l3
	ld	[%i1+16],%l4
	ld	[%i1+20],%l5
	ld	[%i1+24],%l6
	ld	[%i1+28],%l7
	sllx	%l1,%i4,%g5	! Xload(0)
	add	%i4,32,%g3
	sllx	%l0,%g3,%g4
	ld	[%i1+32],%l0
	srlx	%l2,%i5,%l1
	or	%g4,%g5,%g5
	or	%l1,%g5,%g5
	ld	[%i1+36],%l1
	add	%o7,%g5,%g2
	stx	%g5,[%sp+112]
	srlx	%o4,14,%o7	!! 0
	xor	%o5,%g1,%g5
	sllx	%o4,23,%g4
	and	%o4,%g5,%g5
	srlx	%o4,18,%g3
	xor	%g4,%o7,%o7
	sllx	%o4,46,%g4
	xor	%g3,%o7,%o7
	srlx	%o4,41,%g3
	xor	%g4,%o7,%o7
	sllx	%o4,50,%g4
	xor	%g3,%o7,%o7
	xor	%g1,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%o7,%g3		! Sigma1(e)

	srlx	%o0,28,%o7
	add	%g5,%g2,%g2
	ldx	[%i3+0],%g5	! K[0]
	sllx	%o0,25,%g4
	add	%g3,%g2,%g2
	srlx	%o0,34,%g3
	xor	%g4,%o7,%o7
	sllx	%o0,30,%g4
	xor	%g3,%o7,%o7
	srlx	%o0,39,%g3
	xor	%g4,%o7,%o7	
	sllx	%o0,36,%g4
	xor	%g3,%o7,%o7
	xor	%g4,%o7,%o7		! Sigma0(a)

	or	%o0,%o1,%g3
	and	%o0,%o1,%g4
	and	%o2,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[0]
	add	%g4,%o7,%o7

	add	%g2,%o3,%o3
	add	%g2,%o7,%o7
	sllx	%l3,%i4,%g5	! Xload(1)
	add	%i4,32,%g3
	sllx	%l2,%g3,%g4
	ld	[%i1+40],%l2
	srlx	%l4,%i5,%l3
	or	%g4,%g5,%g5
	or	%l3,%g5,%g5
	ld	[%i1+44],%l3
	add	%g1,%g5,%g2
	stx	%g5,[%sp+120]
	srlx	%o3,14,%g1	!! 1
	xor	%o4,%o5,%g5
	sllx	%o3,23,%g4
	and	%o3,%g5,%g5
	srlx	%o3,18,%g3
	xor	%g4,%g1,%g1
	sllx	%o3,46,%g4
	xor	%g3,%g1,%g1
	srlx	%o3,41,%g3
	xor	%g4,%g1,%g1
	sllx	%o3,50,%g4
	xor	%g3,%g1,%g1
	xor	%o5,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%g1,%g3		! Sigma1(e)

	srlx	%o7,28,%g1
	add	%g5,%g2,%g2
	ldx	[%i3+8],%g5	! K[1]
	sllx	%o7,25,%g4
	add	%g3,%g2,%g2
	srlx	%o7,34,%g3
	xor	%g4,%g1,%g1
	sllx	%o7,30,%g4
	xor	%g3,%g1,%g1
	srlx	%o7,39,%g3
	xor	%g4,%g1,%g1	
	sllx	%o7,36,%g4
	xor	%g3,%g1,%g1
	xor	%g4,%g1,%g1		! Sigma0(a)

	or	%o7,%o0,%g3
	and	%o7,%o0,%g4
	and	%o1,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[1]
	add	%g4,%g1,%g1

	add	%g2,%o2,%o2
	add	%g2,%g1,%g1
	sllx	%l5,%i4,%g5	! Xload(2)
	add	%i4,32,%g3
	sllx	%l4,%g3,%g4
	ld	[%i1+48],%l4
	srlx	%l6,%i5,%l5
	or	%g4,%g5,%g5
	or	%l5,%g5,%g5
	ld	[%i1+52],%l5
	add	%o5,%g5,%g2
	stx	%g5,[%sp+128]
	srlx	%o2,14,%o5	!! 2
	xor	%o3,%o4,%g5
	sllx	%o2,23,%g4
	and	%o2,%g5,%g5
	srlx	%o2,18,%g3
	xor	%g4,%o5,%o5
	sllx	%o2,46,%g4
	xor	%g3,%o5,%o5
	srlx	%o2,41,%g3
	xor	%g4,%o5,%o5
	sllx	%o2,50,%g4
	xor	%g3,%o5,%o5
	xor	%o4,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%o5,%g3		! Sigma1(e)

	srlx	%g1,28,%o5
	add	%g5,%g2,%g2
	ldx	[%i3+16],%g5	! K[2]
	sllx	%g1,25,%g4
	add	%g3,%g2,%g2
	srlx	%g1,34,%g3
	xor	%g4,%o5,%o5
	sllx	%g1,30,%g4
	xor	%g3,%o5,%o5
	srlx	%g1,39,%g3
	xor	%g4,%o5,%o5	
	sllx	%g1,36,%g4
	xor	%g3,%o5,%o5
	xor	%g4,%o5,%o5		! Sigma0(a)

	or	%g1,%o7,%g3
	and	%g1,%o7,%g4
	and	%o0,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[2]
	add	%g4,%o5,%o5

	add	%g2,%o1,%o1
	add	%g2,%o5,%o5
	sllx	%l7,%i4,%g5	! Xload(3)
	add	%i4,32,%g3
	sllx	%l6,%g3,%g4
	ld	[%i1+56],%l6
	srlx	%l0,%i5,%l7
	or	%g4,%g5,%g5
	or	%l7,%g5,%g5
	ld	[%i1+60],%l7
	add	%o4,%g5,%g2
	stx	%g5,[%sp+136]
	srlx	%o1,14,%o4	!! 3
	xor	%o2,%o3,%g5
	sllx	%o1,23,%g4
	and	%o1,%g5,%g5
	srlx	%o1,18,%g3
	xor	%g4,%o4,%o4
	sllx	%o1,46,%g4
	xor	%g3,%o4,%o4
	srlx	%o1,41,%g3
	xor	%g4,%o4,%o4
	sllx	%o1,50,%g4
	xor	%g3,%o4,%o4
	xor	%o3,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%o4,%g3		! Sigma1(e)

	srlx	%o5,28,%o4
	add	%g5,%g2,%g2
	ldx	[%i3+24],%g5	! K[3]
	sllx	%o5,25,%g4
	add	%g3,%g2,%g2
	srlx	%o5,34,%g3
	xor	%g4,%o4,%o4
	sllx	%o5,30,%g4
	xor	%g3,%o4,%o4
	srlx	%o5,39,%g3
	xor	%g4,%o4,%o4	
	sllx	%o5,36,%g4
	xor	%g3,%o4,%o4
	xor	%g4,%o4,%o4		! Sigma0(a)

	or	%o5,%g1,%g3
	and	%o5,%g1,%g4
	and	%o7,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[3]
	add	%g4,%o4,%o4

	add	%g2,%o0,%o0
	add	%g2,%o4,%o4
	sllx	%l1,%i4,%g5	! Xload(4)
	add	%i4,32,%g3
	sllx	%l0,%g3,%g4
	ld	[%i1+64],%l0
	srlx	%l2,%i5,%l1
	or	%g4,%g5,%g5
	or	%l1,%g5,%g5
	ld	[%i1+68],%l1
	add	%o3,%g5,%g2
	stx	%g5,[%sp+144]
	srlx	%o0,14,%o3	!! 4
	xor	%o1,%o2,%g5
	sllx	%o0,23,%g4
	and	%o0,%g5,%g5
	srlx	%o0,18,%g3
	xor	%g4,%o3,%o3
	sllx	%o0,46,%g4
	xor	%g3,%o3,%o3
	srlx	%o0,41,%g3
	xor	%g4,%o3,%o3
	sllx	%o0,50,%g4
	xor	%g3,%o3,%o3
	xor	%o2,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%o3,%g3		! Sigma1(e)

	srlx	%o4,28,%o3
	add	%g5,%g2,%g2
	ldx	[%i3+32],%g5	! K[4]
	sllx	%o4,25,%g4
	add	%g3,%g2,%g2
	srlx	%o4,34,%g3
	xor	%g4,%o3,%o3
	sllx	%o4,30,%g4
	xor	%g3,%o3,%o3
	srlx	%o4,39,%g3
	xor	%g4,%o3,%o3	
	sllx	%o4,36,%g4
	xor	%g3,%o3,%o3
	xor	%g4,%o3,%o3		! Sigma0(a)

	or	%o4,%o5,%g3
	and	%o4,%o5,%g4
	and	%g1,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[4]
	add	%g4,%o3,%o3

	add	%g2,%o7,%o7
	add	%g2,%o3,%o3
	sllx	%l3,%i4,%g5	! Xload(5)
	add	%i4,32,%g3
	sllx	%l2,%g3,%g4
	ld	[%i1+72],%l2
	srlx	%l4,%i5,%l3
	or	%g4,%g5,%g5
	or	%l3,%g5,%g5
	ld	[%i1+76],%l3
	add	%o2,%g5,%g2
	stx	%g5,[%sp+152]
	srlx	%o7,14,%o2	!! 5
	xor	%o0,%o1,%g5
	sllx	%o7,23,%g4
	and	%o7,%g5,%g5
	srlx	%o7,18,%g3
	xor	%g4,%o2,%o2
	sllx	%o7,46,%g4
	xor	%g3,%o2,%o2
	srlx	%o7,41,%g3
	xor	%g4,%o2,%o2
	sllx	%o7,50,%g4
	xor	%g3,%o2,%o2
	xor	%o1,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%o2,%g3		! Sigma1(e)

	srlx	%o3,28,%o2
	add	%g5,%g2,%g2
	ldx	[%i3+40],%g5	! K[5]
	sllx	%o3,25,%g4
	add	%g3,%g2,%g2
	srlx	%o3,34,%g3
	xor	%g4,%o2,%o2
	sllx	%o3,30,%g4
	xor	%g3,%o2,%o2
	srlx	%o3,39,%g3
	xor	%g4,%o2,%o2	
	sllx	%o3,36,%g4
	xor	%g3,%o2,%o2
	xor	%g4,%o2,%o2		! Sigma0(a)

	or	%o3,%o4,%g3
	and	%o3,%o4,%g4
	and	%o5,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[5]
	add	%g4,%o2,%o2

	add	%g2,%g1,%g1
	add	%g2,%o2,%o2
	sllx	%l5,%i4,%g5	! Xload(6)
	add	%i4,32,%g3
	sllx	%l4,%g3,%g4
	ld	[%i1+80],%l4
	srlx	%l6,%i5,%l5
	or	%g4,%g5,%g5
	or	%l5,%g5,%g5
	ld	[%i1+84],%l5
	add	%o1,%g5,%g2
	stx	%g5,[%sp+160]
	srlx	%g1,14,%o1	!! 6
	xor	%o7,%o0,%g5
	sllx	%g1,23,%g4
	and	%g1,%g5,%g5
	srlx	%g1,18,%g3
	xor	%g4,%o1,%o1
	sllx	%g1,46,%g4
	xor	%g3,%o1,%o1
	srlx	%g1,41,%g3
	xor	%g4,%o1,%o1
	sllx	%g1,50,%g4
	xor	%g3,%o1,%o1
	xor	%o0,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%o1,%g3		! Sigma1(e)

	srlx	%o2,28,%o1
	add	%g5,%g2,%g2
	ldx	[%i3+48],%g5	! K[6]
	sllx	%o2,25,%g4
	add	%g3,%g2,%g2
	srlx	%o2,34,%g3
	xor	%g4,%o1,%o1
	sllx	%o2,30,%g4
	xor	%g3,%o1,%o1
	srlx	%o2,39,%g3
	xor	%g4,%o1,%o1	
	sllx	%o2,36,%g4
	xor	%g3,%o1,%o1
	xor	%g4,%o1,%o1		! Sigma0(a)

	or	%o2,%o3,%g3
	and	%o2,%o3,%g4
	and	%o4,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[6]
	add	%g4,%o1,%o1

	add	%g2,%o5,%o5
	add	%g2,%o1,%o1
	sllx	%l7,%i4,%g5	! Xload(7)
	add	%i4,32,%g3
	sllx	%l6,%g3,%g4
	ld	[%i1+88],%l6
	srlx	%l0,%i5,%l7
	or	%g4,%g5,%g5
	or	%l7,%g5,%g5
	ld	[%i1+92],%l7
	add	%o0,%g5,%g2
	stx	%g5,[%sp+168]
	srlx	%o5,14,%o0	!! 7
	xor	%g1,%o7,%g5
	sllx	%o5,23,%g4
	and	%o5,%g5,%g5
	srlx	%o5,18,%g3
	xor	%g4,%o0,%o0
	sllx	%o5,46,%g4
	xor	%g3,%o0,%o0
	srlx	%o5,41,%g3
	xor	%g4,%o0,%o0
	sllx	%o5,50,%g4
	xor	%g3,%o0,%o0
	xor	%o7,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%o0,%g3		! Sigma1(e)

	srlx	%o1,28,%o0
	add	%g5,%g2,%g2
	ldx	[%i3+56],%g5	! K[7]
	sllx	%o1,25,%g4
	add	%g3,%g2,%g2
	srlx	%o1,34,%g3
	xor	%g4,%o0,%o0
	sllx	%o1,30,%g4
	xor	%g3,%o0,%o0
	srlx	%o1,39,%g3
	xor	%g4,%o0,%o0	
	sllx	%o1,36,%g4
	xor	%g3,%o0,%o0
	xor	%g4,%o0,%o0		! Sigma0(a)

	or	%o1,%o2,%g3
	and	%o1,%o2,%g4
	and	%o3,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[7]
	add	%g4,%o0,%o0

	add	%g2,%o4,%o4
	add	%g2,%o0,%o0
	sllx	%l1,%i4,%g5	! Xload(8)
	add	%i4,32,%g3
	sllx	%l0,%g3,%g4
	ld	[%i1+96],%l0
	srlx	%l2,%i5,%l1
	or	%g4,%g5,%g5
	or	%l1,%g5,%g5
	ld	[%i1+100],%l1
	add	%o7,%g5,%g2
	stx	%g5,[%sp+176]
	srlx	%o4,14,%o7	!! 8
	xor	%o5,%g1,%g5
	sllx	%o4,23,%g4
	and	%o4,%g5,%g5
	srlx	%o4,18,%g3
	xor	%g4,%o7,%o7
	sllx	%o4,46,%g4
	xor	%g3,%o7,%o7
	srlx	%o4,41,%g3
	xor	%g4,%o7,%o7
	sllx	%o4,50,%g4
	xor	%g3,%o7,%o7
	xor	%g1,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%o7,%g3		! Sigma1(e)

	srlx	%o0,28,%o7
	add	%g5,%g2,%g2
	ldx	[%i3+64],%g5	! K[8]
	sllx	%o0,25,%g4
	add	%g3,%g2,%g2
	srlx	%o0,34,%g3
	xor	%g4,%o7,%o7
	sllx	%o0,30,%g4
	xor	%g3,%o7,%o7
	srlx	%o0,39,%g3
	xor	%g4,%o7,%o7	
	sllx	%o0,36,%g4
	xor	%g3,%o7,%o7
	xor	%g4,%o7,%o7		! Sigma0(a)

	or	%o0,%o1,%g3
	and	%o0,%o1,%g4
	and	%o2,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[8]
	add	%g4,%o7,%o7

	add	%g2,%o3,%o3
	add	%g2,%o7,%o7
	sllx	%l3,%i4,%g5	! Xload(9)
	add	%i4,32,%g3
	sllx	%l2,%g3,%g4
	ld	[%i1+104],%l2
	srlx	%l4,%i5,%l3
	or	%g4,%g5,%g5
	or	%l3,%g5,%g5
	ld	[%i1+108],%l3
	add	%g1,%g5,%g2
	stx	%g5,[%sp+184]
	srlx	%o3,14,%g1	!! 9
	xor	%o4,%o5,%g5
	sllx	%o3,23,%g4
	and	%o3,%g5,%g5
	srlx	%o3,18,%g3
	xor	%g4,%g1,%g1
	sllx	%o3,46,%g4
	xor	%g3,%g1,%g1
	srlx	%o3,41,%g3
	xor	%g4,%g1,%g1
	sllx	%o3,50,%g4
	xor	%g3,%g1,%g1
	xor	%o5,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%g1,%g3		! Sigma1(e)

	srlx	%o7,28,%g1
	add	%g5,%g2,%g2
	ldx	[%i3+72],%g5	! K[9]
	sllx	%o7,25,%g4
	add	%g3,%g2,%g2
	srlx	%o7,34,%g3
	xor	%g4,%g1,%g1
	sllx	%o7,30,%g4
	xor	%g3,%g1,%g1
	srlx	%o7,39,%g3
	xor	%g4,%g1,%g1	
	sllx	%o7,36,%g4
	xor	%g3,%g1,%g1
	xor	%g4,%g1,%g1		! Sigma0(a)

	or	%o7,%o0,%g3
	and	%o7,%o0,%g4
	and	%o1,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[9]
	add	%g4,%g1,%g1

	add	%g2,%o2,%o2
	add	%g2,%g1,%g1
	sllx	%l5,%i4,%g5	! Xload(10)
	add	%i4,32,%g3
	sllx	%l4,%g3,%g4
	ld	[%i1+112],%l4
	srlx	%l6,%i5,%l5
	or	%g4,%g5,%g5
	or	%l5,%g5,%g5
	ld	[%i1+116],%l5
	add	%o5,%g5,%g2
	stx	%g5,[%sp+192]
	srlx	%o2,14,%o5	!! 10
	xor	%o3,%o4,%g5
	sllx	%o2,23,%g4
	and	%o2,%g5,%g5
	srlx	%o2,18,%g3
	xor	%g4,%o5,%o5
	sllx	%o2,46,%g4
	xor	%g3,%o5,%o5
	srlx	%o2,41,%g3
	xor	%g4,%o5,%o5
	sllx	%o2,50,%g4
	xor	%g3,%o5,%o5
	xor	%o4,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%o5,%g3		! Sigma1(e)

	srlx	%g1,28,%o5
	add	%g5,%g2,%g2
	ldx	[%i3+80],%g5	! K[10]
	sllx	%g1,25,%g4
	add	%g3,%g2,%g2
	srlx	%g1,34,%g3
	xor	%g4,%o5,%o5
	sllx	%g1,30,%g4
	xor	%g3,%o5,%o5
	srlx	%g1,39,%g3
	xor	%g4,%o5,%o5	
	sllx	%g1,36,%g4
	xor	%g3,%o5,%o5
	xor	%g4,%o5,%o5		! Sigma0(a)

	or	%g1,%o7,%g3
	and	%g1,%o7,%g4
	and	%o0,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[10]
	add	%g4,%o5,%o5

	add	%g2,%o1,%o1
	add	%g2,%o5,%o5
	sllx	%l7,%i4,%g5	! Xload(11)
	add	%i4,32,%g3
	sllx	%l6,%g3,%g4
	ld	[%i1+120],%l6
	srlx	%l0,%i5,%l7
	or	%g4,%g5,%g5
	or	%l7,%g5,%g5
	ld	[%i1+124],%l7
	add	%o4,%g5,%g2
	stx	%g5,[%sp+200]
	srlx	%o1,14,%o4	!! 11
	xor	%o2,%o3,%g5
	sllx	%o1,23,%g4
	and	%o1,%g5,%g5
	srlx	%o1,18,%g3
	xor	%g4,%o4,%o4
	sllx	%o1,46,%g4
	xor	%g3,%o4,%o4
	srlx	%o1,41,%g3
	xor	%g4,%o4,%o4
	sllx	%o1,50,%g4
	xor	%g3,%o4,%o4
	xor	%o3,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%o4,%g3		! Sigma1(e)

	srlx	%o5,28,%o4
	add	%g5,%g2,%g2
	ldx	[%i3+88],%g5	! K[11]
	sllx	%o5,25,%g4
	add	%g3,%g2,%g2
	srlx	%o5,34,%g3
	xor	%g4,%o4,%o4
	sllx	%o5,30,%g4
	xor	%g3,%o4,%o4
	srlx	%o5,39,%g3
	xor	%g4,%o4,%o4	
	sllx	%o5,36,%g4
	xor	%g3,%o4,%o4
	xor	%g4,%o4,%o4		! Sigma0(a)

	or	%o5,%g1,%g3
	and	%o5,%g1,%g4
	and	%o7,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[11]
	add	%g4,%o4,%o4

	add	%g2,%o0,%o0
	add	%g2,%o4,%o4
	sllx	%l1,%i4,%g5	! Xload(12)
	add	%i4,32,%g3
	sllx	%l0,%g3,%g4
	
	srlx	%l2,%i5,%l1
	or	%g4,%g5,%g5
	or	%l1,%g5,%g5
	
	add	%o3,%g5,%g2
	stx	%g5,[%sp+208]
	brnz,a	%i4,.+8
	ld	[%i1+128],%l0
	srlx	%o0,14,%o3	!! 12
	xor	%o1,%o2,%g5
	sllx	%o0,23,%g4
	and	%o0,%g5,%g5
	srlx	%o0,18,%g3
	xor	%g4,%o3,%o3
	sllx	%o0,46,%g4
	xor	%g3,%o3,%o3
	srlx	%o0,41,%g3
	xor	%g4,%o3,%o3
	sllx	%o0,50,%g4
	xor	%g3,%o3,%o3
	xor	%o2,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%o3,%g3		! Sigma1(e)

	srlx	%o4,28,%o3
	add	%g5,%g2,%g2
	ldx	[%i3+96],%g5	! K[12]
	sllx	%o4,25,%g4
	add	%g3,%g2,%g2
	srlx	%o4,34,%g3
	xor	%g4,%o3,%o3
	sllx	%o4,30,%g4
	xor	%g3,%o3,%o3
	srlx	%o4,39,%g3
	xor	%g4,%o3,%o3	
	sllx	%o4,36,%g4
	xor	%g3,%o3,%o3
	xor	%g4,%o3,%o3		! Sigma0(a)

	or	%o4,%o5,%g3
	and	%o4,%o5,%g4
	and	%g1,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[12]
	add	%g4,%o3,%o3

	add	%g2,%o7,%o7
	add	%g2,%o3,%o3
	sllx	%l3,%i4,%g5	! Xload(13)
	add	%i4,32,%g3
	sllx	%l2,%g3,%g4
	
	srlx	%l4,%i5,%l3
	or	%g4,%g5,%g5
	or	%l3,%g5,%g5
	
	add	%o2,%g5,%g2
	stx	%g5,[%sp+216]
	srlx	%o7,14,%o2	!! 13
	xor	%o0,%o1,%g5
	sllx	%o7,23,%g4
	and	%o7,%g5,%g5
	srlx	%o7,18,%g3
	xor	%g4,%o2,%o2
	sllx	%o7,46,%g4
	xor	%g3,%o2,%o2
	srlx	%o7,41,%g3
	xor	%g4,%o2,%o2
	sllx	%o7,50,%g4
	xor	%g3,%o2,%o2
	xor	%o1,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%o2,%g3		! Sigma1(e)

	srlx	%o3,28,%o2
	add	%g5,%g2,%g2
	ldx	[%i3+104],%g5	! K[13]
	sllx	%o3,25,%g4
	add	%g3,%g2,%g2
	srlx	%o3,34,%g3
	xor	%g4,%o2,%o2
	sllx	%o3,30,%g4
	xor	%g3,%o2,%o2
	srlx	%o3,39,%g3
	xor	%g4,%o2,%o2	
	sllx	%o3,36,%g4
	xor	%g3,%o2,%o2
	xor	%g4,%o2,%o2		! Sigma0(a)

	or	%o3,%o4,%g3
	and	%o3,%o4,%g4
	and	%o5,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[13]
	add	%g4,%o2,%o2

	add	%g2,%g1,%g1
	add	%g2,%o2,%o2
	sllx	%l5,%i4,%g5	! Xload(14)
	add	%i4,32,%g3
	sllx	%l4,%g3,%g4
	
	srlx	%l6,%i5,%l5
	or	%g4,%g5,%g5
	or	%l5,%g5,%g5
	
	add	%o1,%g5,%g2
	stx	%g5,[%sp+224]
	srlx	%g1,14,%o1	!! 14
	xor	%o7,%o0,%g5
	sllx	%g1,23,%g4
	and	%g1,%g5,%g5
	srlx	%g1,18,%g3
	xor	%g4,%o1,%o1
	sllx	%g1,46,%g4
	xor	%g3,%o1,%o1
	srlx	%g1,41,%g3
	xor	%g4,%o1,%o1
	sllx	%g1,50,%g4
	xor	%g3,%o1,%o1
	xor	%o0,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%o1,%g3		! Sigma1(e)

	srlx	%o2,28,%o1
	add	%g5,%g2,%g2
	ldx	[%i3+112],%g5	! K[14]
	sllx	%o2,25,%g4
	add	%g3,%g2,%g2
	srlx	%o2,34,%g3
	xor	%g4,%o1,%o1
	sllx	%o2,30,%g4
	xor	%g3,%o1,%o1
	srlx	%o2,39,%g3
	xor	%g4,%o1,%o1	
	sllx	%o2,36,%g4
	xor	%g3,%o1,%o1
	xor	%g4,%o1,%o1		! Sigma0(a)

	or	%o2,%o3,%g3
	and	%o2,%o3,%g4
	and	%o4,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[14]
	add	%g4,%o1,%o1

	add	%g2,%o5,%o5
	add	%g2,%o1,%o1
	ld	[%sp+120],%l2
	sllx	%l7,%i4,%g5	! Xload(15)
	add	%i4,32,%g3
	ld	[%sp+124],%l3
	sllx	%l6,%g3,%g4
	ld	[%sp+184],%l4
	srlx	%l0,%i5,%l7
	or	%g4,%g5,%g5
	ld	[%sp+188],%l5
	or	%l7,%g5,%g5
	ld	[%sp+224],%l6
	add	%o0,%g5,%g2
	stx	%g5,[%sp+232]
	ld	[%sp+228],%l7
	ld	[%sp+112],%l0
	ld	[%sp+116],%l1
	srlx	%o5,14,%o0	!! 15
	xor	%g1,%o7,%g5
	sllx	%o5,23,%g4
	and	%o5,%g5,%g5
	srlx	%o5,18,%g3
	xor	%g4,%o0,%o0
	sllx	%o5,46,%g4
	xor	%g3,%o0,%o0
	srlx	%o5,41,%g3
	xor	%g4,%o0,%o0
	sllx	%o5,50,%g4
	xor	%g3,%o0,%o0
	xor	%o7,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%o0,%g3		! Sigma1(e)

	srlx	%o1,28,%o0
	add	%g5,%g2,%g2
	ldx	[%i3+120],%g5	! K[15]
	sllx	%o1,25,%g4
	add	%g3,%g2,%g2
	srlx	%o1,34,%g3
	xor	%g4,%o0,%o0
	sllx	%o1,30,%g4
	xor	%g3,%o0,%o0
	srlx	%o1,39,%g3
	xor	%g4,%o0,%o0	
	sllx	%o1,36,%g4
	xor	%g3,%o0,%o0
	xor	%g4,%o0,%o0		! Sigma0(a)

	or	%o1,%o2,%g3
	and	%o1,%o2,%g4
	and	%o3,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[15]
	add	%g4,%o0,%o0

	add	%g2,%o4,%o4
	add	%g2,%o0,%o0
.L16_xx:
	sllx	%l2,32,%g3		!! Xupdate(16)
	or	%l3,%g3,%g3

	srlx	%g3,7,%g2
	ld	[%sp+128],%l2
	sllx	%g3,56,%g4
	ld	[%sp+132],%l3
	srlx	%g3,1,%g3
	xor	%g4,%g2,%g2
	sllx	%g4,7,%g4
	xor	%g3,%g2,%g2
	srlx	%g3,7,%g3
	xor	%g4,%g2,%g2
	sllx	%l6,32,%g5
	xor	%g3,%g2,%g2		! sigma0(X[16+1])
	or	%l7,%g5,%g5

	srlx	%g5,6,%g4
	ld	[%sp+232],%l6
	sllx	%g5,3,%g3
	ld	[%sp+236],%l7
	srlx	%g5,19,%g5
	xor	%g3,%g4,%g4
	sllx	%g3,42,%g3
	xor	%g5,%g4,%g4
	srlx	%g5,42,%g5
	xor	%g3,%g4,%g4
	sllx	%l4,32,%g3
	xor	%g5,%g4,%g4	! sigma1(X[16+14])
	ld	[%sp+192],%l4
	or	%l5,%g3,%g3
	ld	[%sp+196],%l5

	sllx	%l0,32,%g5
	add	%g4,%g2,%g2
	ld	[%sp+120],%l0
	or	%l1,%g5,%g5
	add	%g3,%g2,%g2		! +=X[16+9]
	ld	[%sp+124],%l1
	add	%g5,%g2,%g2		! +=X[16]
	stx	%g2,[%sp+112]
	add	%o7,%g2,%g2
	srlx	%o4,14,%o7	!! 16
	xor	%o5,%g1,%g5
	sllx	%o4,23,%g4
	and	%o4,%g5,%g5
	srlx	%o4,18,%g3
	xor	%g4,%o7,%o7
	sllx	%o4,46,%g4
	xor	%g3,%o7,%o7
	srlx	%o4,41,%g3
	xor	%g4,%o7,%o7
	sllx	%o4,50,%g4
	xor	%g3,%o7,%o7
	xor	%g1,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%o7,%g3		! Sigma1(e)

	srlx	%o0,28,%o7
	add	%g5,%g2,%g2
	ldx	[%i3+128],%g5	! K[16]
	sllx	%o0,25,%g4
	add	%g3,%g2,%g2
	srlx	%o0,34,%g3
	xor	%g4,%o7,%o7
	sllx	%o0,30,%g4
	xor	%g3,%o7,%o7
	srlx	%o0,39,%g3
	xor	%g4,%o7,%o7	
	sllx	%o0,36,%g4
	xor	%g3,%o7,%o7
	xor	%g4,%o7,%o7		! Sigma0(a)

	or	%o0,%o1,%g3
	and	%o0,%o1,%g4
	and	%o2,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[16]
	add	%g4,%o7,%o7

	add	%g2,%o3,%o3
	add	%g2,%o7,%o7
	sllx	%l2,32,%g3		!! Xupdate(17)
	or	%l3,%g3,%g3

	srlx	%g3,7,%g2
	ld	[%sp+136],%l2
	sllx	%g3,56,%g4
	ld	[%sp+140],%l3
	srlx	%g3,1,%g3
	xor	%g4,%g2,%g2
	sllx	%g4,7,%g4
	xor	%g3,%g2,%g2
	srlx	%g3,7,%g3
	xor	%g4,%g2,%g2
	sllx	%l6,32,%g5
	xor	%g3,%g2,%g2		! sigma0(X[17+1])
	or	%l7,%g5,%g5

	srlx	%g5,6,%g4
	ld	[%sp+112],%l6
	sllx	%g5,3,%g3
	ld	[%sp+116],%l7
	srlx	%g5,19,%g5
	xor	%g3,%g4,%g4
	sllx	%g3,42,%g3
	xor	%g5,%g4,%g4
	srlx	%g5,42,%g5
	xor	%g3,%g4,%g4
	sllx	%l4,32,%g3
	xor	%g5,%g4,%g4	! sigma1(X[17+14])
	ld	[%sp+200],%l4
	or	%l5,%g3,%g3
	ld	[%sp+204],%l5

	sllx	%l0,32,%g5
	add	%g4,%g2,%g2
	ld	[%sp+128],%l0
	or	%l1,%g5,%g5
	add	%g3,%g2,%g2		! +=X[17+9]
	ld	[%sp+132],%l1
	add	%g5,%g2,%g2		! +=X[17]
	stx	%g2,[%sp+120]
	add	%g1,%g2,%g2
	srlx	%o3,14,%g1	!! 17
	xor	%o4,%o5,%g5
	sllx	%o3,23,%g4
	and	%o3,%g5,%g5
	srlx	%o3,18,%g3
	xor	%g4,%g1,%g1
	sllx	%o3,46,%g4
	xor	%g3,%g1,%g1
	srlx	%o3,41,%g3
	xor	%g4,%g1,%g1
	sllx	%o3,50,%g4
	xor	%g3,%g1,%g1
	xor	%o5,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%g1,%g3		! Sigma1(e)

	srlx	%o7,28,%g1
	add	%g5,%g2,%g2
	ldx	[%i3+136],%g5	! K[17]
	sllx	%o7,25,%g4
	add	%g3,%g2,%g2
	srlx	%o7,34,%g3
	xor	%g4,%g1,%g1
	sllx	%o7,30,%g4
	xor	%g3,%g1,%g1
	srlx	%o7,39,%g3
	xor	%g4,%g1,%g1	
	sllx	%o7,36,%g4
	xor	%g3,%g1,%g1
	xor	%g4,%g1,%g1		! Sigma0(a)

	or	%o7,%o0,%g3
	and	%o7,%o0,%g4
	and	%o1,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[17]
	add	%g4,%g1,%g1

	add	%g2,%o2,%o2
	add	%g2,%g1,%g1
	sllx	%l2,32,%g3		!! Xupdate(18)
	or	%l3,%g3,%g3

	srlx	%g3,7,%g2
	ld	[%sp+144],%l2
	sllx	%g3,56,%g4
	ld	[%sp+148],%l3
	srlx	%g3,1,%g3
	xor	%g4,%g2,%g2
	sllx	%g4,7,%g4
	xor	%g3,%g2,%g2
	srlx	%g3,7,%g3
	xor	%g4,%g2,%g2
	sllx	%l6,32,%g5
	xor	%g3,%g2,%g2		! sigma0(X[18+1])
	or	%l7,%g5,%g5

	srlx	%g5,6,%g4
	ld	[%sp+120],%l6
	sllx	%g5,3,%g3
	ld	[%sp+124],%l7
	srlx	%g5,19,%g5
	xor	%g3,%g4,%g4
	sllx	%g3,42,%g3
	xor	%g5,%g4,%g4
	srlx	%g5,42,%g5
	xor	%g3,%g4,%g4
	sllx	%l4,32,%g3
	xor	%g5,%g4,%g4	! sigma1(X[18+14])
	ld	[%sp+208],%l4
	or	%l5,%g3,%g3
	ld	[%sp+212],%l5

	sllx	%l0,32,%g5
	add	%g4,%g2,%g2
	ld	[%sp+136],%l0
	or	%l1,%g5,%g5
	add	%g3,%g2,%g2		! +=X[18+9]
	ld	[%sp+140],%l1
	add	%g5,%g2,%g2		! +=X[18]
	stx	%g2,[%sp+128]
	add	%o5,%g2,%g2
	srlx	%o2,14,%o5	!! 18
	xor	%o3,%o4,%g5
	sllx	%o2,23,%g4
	and	%o2,%g5,%g5
	srlx	%o2,18,%g3
	xor	%g4,%o5,%o5
	sllx	%o2,46,%g4
	xor	%g3,%o5,%o5
	srlx	%o2,41,%g3
	xor	%g4,%o5,%o5
	sllx	%o2,50,%g4
	xor	%g3,%o5,%o5
	xor	%o4,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%o5,%g3		! Sigma1(e)

	srlx	%g1,28,%o5
	add	%g5,%g2,%g2
	ldx	[%i3+144],%g5	! K[18]
	sllx	%g1,25,%g4
	add	%g3,%g2,%g2
	srlx	%g1,34,%g3
	xor	%g4,%o5,%o5
	sllx	%g1,30,%g4
	xor	%g3,%o5,%o5
	srlx	%g1,39,%g3
	xor	%g4,%o5,%o5	
	sllx	%g1,36,%g4
	xor	%g3,%o5,%o5
	xor	%g4,%o5,%o5		! Sigma0(a)

	or	%g1,%o7,%g3
	and	%g1,%o7,%g4
	and	%o0,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[18]
	add	%g4,%o5,%o5

	add	%g2,%o1,%o1
	add	%g2,%o5,%o5
	sllx	%l2,32,%g3		!! Xupdate(19)
	or	%l3,%g3,%g3

	srlx	%g3,7,%g2
	ld	[%sp+152],%l2
	sllx	%g3,56,%g4
	ld	[%sp+156],%l3
	srlx	%g3,1,%g3
	xor	%g4,%g2,%g2
	sllx	%g4,7,%g4
	xor	%g3,%g2,%g2
	srlx	%g3,7,%g3
	xor	%g4,%g2,%g2
	sllx	%l6,32,%g5
	xor	%g3,%g2,%g2		! sigma0(X[19+1])
	or	%l7,%g5,%g5

	srlx	%g5,6,%g4
	ld	[%sp+128],%l6
	sllx	%g5,3,%g3
	ld	[%sp+132],%l7
	srlx	%g5,19,%g5
	xor	%g3,%g4,%g4
	sllx	%g3,42,%g3
	xor	%g5,%g4,%g4
	srlx	%g5,42,%g5
	xor	%g3,%g4,%g4
	sllx	%l4,32,%g3
	xor	%g5,%g4,%g4	! sigma1(X[19+14])
	ld	[%sp+216],%l4
	or	%l5,%g3,%g3
	ld	[%sp+220],%l5

	sllx	%l0,32,%g5
	add	%g4,%g2,%g2
	ld	[%sp+144],%l0
	or	%l1,%g5,%g5
	add	%g3,%g2,%g2		! +=X[19+9]
	ld	[%sp+148],%l1
	add	%g5,%g2,%g2		! +=X[19]
	stx	%g2,[%sp+136]
	add	%o4,%g2,%g2
	srlx	%o1,14,%o4	!! 19
	xor	%o2,%o3,%g5
	sllx	%o1,23,%g4
	and	%o1,%g5,%g5
	srlx	%o1,18,%g3
	xor	%g4,%o4,%o4
	sllx	%o1,46,%g4
	xor	%g3,%o4,%o4
	srlx	%o1,41,%g3
	xor	%g4,%o4,%o4
	sllx	%o1,50,%g4
	xor	%g3,%o4,%o4
	xor	%o3,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%o4,%g3		! Sigma1(e)

	srlx	%o5,28,%o4
	add	%g5,%g2,%g2
	ldx	[%i3+152],%g5	! K[19]
	sllx	%o5,25,%g4
	add	%g3,%g2,%g2
	srlx	%o5,34,%g3
	xor	%g4,%o4,%o4
	sllx	%o5,30,%g4
	xor	%g3,%o4,%o4
	srlx	%o5,39,%g3
	xor	%g4,%o4,%o4	
	sllx	%o5,36,%g4
	xor	%g3,%o4,%o4
	xor	%g4,%o4,%o4		! Sigma0(a)

	or	%o5,%g1,%g3
	and	%o5,%g1,%g4
	and	%o7,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[19]
	add	%g4,%o4,%o4

	add	%g2,%o0,%o0
	add	%g2,%o4,%o4
	sllx	%l2,32,%g3		!! Xupdate(20)
	or	%l3,%g3,%g3

	srlx	%g3,7,%g2
	ld	[%sp+160],%l2
	sllx	%g3,56,%g4
	ld	[%sp+164],%l3
	srlx	%g3,1,%g3
	xor	%g4,%g2,%g2
	sllx	%g4,7,%g4
	xor	%g3,%g2,%g2
	srlx	%g3,7,%g3
	xor	%g4,%g2,%g2
	sllx	%l6,32,%g5
	xor	%g3,%g2,%g2		! sigma0(X[20+1])
	or	%l7,%g5,%g5

	srlx	%g5,6,%g4
	ld	[%sp+136],%l6
	sllx	%g5,3,%g3
	ld	[%sp+140],%l7
	srlx	%g5,19,%g5
	xor	%g3,%g4,%g4
	sllx	%g3,42,%g3
	xor	%g5,%g4,%g4
	srlx	%g5,42,%g5
	xor	%g3,%g4,%g4
	sllx	%l4,32,%g3
	xor	%g5,%g4,%g4	! sigma1(X[20+14])
	ld	[%sp+224],%l4
	or	%l5,%g3,%g3
	ld	[%sp+228],%l5

	sllx	%l0,32,%g5
	add	%g4,%g2,%g2
	ld	[%sp+152],%l0
	or	%l1,%g5,%g5
	add	%g3,%g2,%g2		! +=X[20+9]
	ld	[%sp+156],%l1
	add	%g5,%g2,%g2		! +=X[20]
	stx	%g2,[%sp+144]
	add	%o3,%g2,%g2
	srlx	%o0,14,%o3	!! 20
	xor	%o1,%o2,%g5
	sllx	%o0,23,%g4
	and	%o0,%g5,%g5
	srlx	%o0,18,%g3
	xor	%g4,%o3,%o3
	sllx	%o0,46,%g4
	xor	%g3,%o3,%o3
	srlx	%o0,41,%g3
	xor	%g4,%o3,%o3
	sllx	%o0,50,%g4
	xor	%g3,%o3,%o3
	xor	%o2,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%o3,%g3		! Sigma1(e)

	srlx	%o4,28,%o3
	add	%g5,%g2,%g2
	ldx	[%i3+160],%g5	! K[20]
	sllx	%o4,25,%g4
	add	%g3,%g2,%g2
	srlx	%o4,34,%g3
	xor	%g4,%o3,%o3
	sllx	%o4,30,%g4
	xor	%g3,%o3,%o3
	srlx	%o4,39,%g3
	xor	%g4,%o3,%o3	
	sllx	%o4,36,%g4
	xor	%g3,%o3,%o3
	xor	%g4,%o3,%o3		! Sigma0(a)

	or	%o4,%o5,%g3
	and	%o4,%o5,%g4
	and	%g1,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[20]
	add	%g4,%o3,%o3

	add	%g2,%o7,%o7
	add	%g2,%o3,%o3
	sllx	%l2,32,%g3		!! Xupdate(21)
	or	%l3,%g3,%g3

	srlx	%g3,7,%g2
	ld	[%sp+168],%l2
	sllx	%g3,56,%g4
	ld	[%sp+172],%l3
	srlx	%g3,1,%g3
	xor	%g4,%g2,%g2
	sllx	%g4,7,%g4
	xor	%g3,%g2,%g2
	srlx	%g3,7,%g3
	xor	%g4,%g2,%g2
	sllx	%l6,32,%g5
	xor	%g3,%g2,%g2		! sigma0(X[21+1])
	or	%l7,%g5,%g5

	srlx	%g5,6,%g4
	ld	[%sp+144],%l6
	sllx	%g5,3,%g3
	ld	[%sp+148],%l7
	srlx	%g5,19,%g5
	xor	%g3,%g4,%g4
	sllx	%g3,42,%g3
	xor	%g5,%g4,%g4
	srlx	%g5,42,%g5
	xor	%g3,%g4,%g4
	sllx	%l4,32,%g3
	xor	%g5,%g4,%g4	! sigma1(X[21+14])
	ld	[%sp+232],%l4
	or	%l5,%g3,%g3
	ld	[%sp+236],%l5

	sllx	%l0,32,%g5
	add	%g4,%g2,%g2
	ld	[%sp+160],%l0
	or	%l1,%g5,%g5
	add	%g3,%g2,%g2		! +=X[21+9]
	ld	[%sp+164],%l1
	add	%g5,%g2,%g2		! +=X[21]
	stx	%g2,[%sp+152]
	add	%o2,%g2,%g2
	srlx	%o7,14,%o2	!! 21
	xor	%o0,%o1,%g5
	sllx	%o7,23,%g4
	and	%o7,%g5,%g5
	srlx	%o7,18,%g3
	xor	%g4,%o2,%o2
	sllx	%o7,46,%g4
	xor	%g3,%o2,%o2
	srlx	%o7,41,%g3
	xor	%g4,%o2,%o2
	sllx	%o7,50,%g4
	xor	%g3,%o2,%o2
	xor	%o1,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%o2,%g3		! Sigma1(e)

	srlx	%o3,28,%o2
	add	%g5,%g2,%g2
	ldx	[%i3+168],%g5	! K[21]
	sllx	%o3,25,%g4
	add	%g3,%g2,%g2
	srlx	%o3,34,%g3
	xor	%g4,%o2,%o2
	sllx	%o3,30,%g4
	xor	%g3,%o2,%o2
	srlx	%o3,39,%g3
	xor	%g4,%o2,%o2	
	sllx	%o3,36,%g4
	xor	%g3,%o2,%o2
	xor	%g4,%o2,%o2		! Sigma0(a)

	or	%o3,%o4,%g3
	and	%o3,%o4,%g4
	and	%o5,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[21]
	add	%g4,%o2,%o2

	add	%g2,%g1,%g1
	add	%g2,%o2,%o2
	sllx	%l2,32,%g3		!! Xupdate(22)
	or	%l3,%g3,%g3

	srlx	%g3,7,%g2
	ld	[%sp+176],%l2
	sllx	%g3,56,%g4
	ld	[%sp+180],%l3
	srlx	%g3,1,%g3
	xor	%g4,%g2,%g2
	sllx	%g4,7,%g4
	xor	%g3,%g2,%g2
	srlx	%g3,7,%g3
	xor	%g4,%g2,%g2
	sllx	%l6,32,%g5
	xor	%g3,%g2,%g2		! sigma0(X[22+1])
	or	%l7,%g5,%g5

	srlx	%g5,6,%g4
	ld	[%sp+152],%l6
	sllx	%g5,3,%g3
	ld	[%sp+156],%l7
	srlx	%g5,19,%g5
	xor	%g3,%g4,%g4
	sllx	%g3,42,%g3
	xor	%g5,%g4,%g4
	srlx	%g5,42,%g5
	xor	%g3,%g4,%g4
	sllx	%l4,32,%g3
	xor	%g5,%g4,%g4	! sigma1(X[22+14])
	ld	[%sp+112],%l4
	or	%l5,%g3,%g3
	ld	[%sp+116],%l5

	sllx	%l0,32,%g5
	add	%g4,%g2,%g2
	ld	[%sp+168],%l0
	or	%l1,%g5,%g5
	add	%g3,%g2,%g2		! +=X[22+9]
	ld	[%sp+172],%l1
	add	%g5,%g2,%g2		! +=X[22]
	stx	%g2,[%sp+160]
	add	%o1,%g2,%g2
	srlx	%g1,14,%o1	!! 22
	xor	%o7,%o0,%g5
	sllx	%g1,23,%g4
	and	%g1,%g5,%g5
	srlx	%g1,18,%g3
	xor	%g4,%o1,%o1
	sllx	%g1,46,%g4
	xor	%g3,%o1,%o1
	srlx	%g1,41,%g3
	xor	%g4,%o1,%o1
	sllx	%g1,50,%g4
	xor	%g3,%o1,%o1
	xor	%o0,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%o1,%g3		! Sigma1(e)

	srlx	%o2,28,%o1
	add	%g5,%g2,%g2
	ldx	[%i3+176],%g5	! K[22]
	sllx	%o2,25,%g4
	add	%g3,%g2,%g2
	srlx	%o2,34,%g3
	xor	%g4,%o1,%o1
	sllx	%o2,30,%g4
	xor	%g3,%o1,%o1
	srlx	%o2,39,%g3
	xor	%g4,%o1,%o1	
	sllx	%o2,36,%g4
	xor	%g3,%o1,%o1
	xor	%g4,%o1,%o1		! Sigma0(a)

	or	%o2,%o3,%g3
	and	%o2,%o3,%g4
	and	%o4,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[22]
	add	%g4,%o1,%o1

	add	%g2,%o5,%o5
	add	%g2,%o1,%o1
	sllx	%l2,32,%g3		!! Xupdate(23)
	or	%l3,%g3,%g3

	srlx	%g3,7,%g2
	ld	[%sp+184],%l2
	sllx	%g3,56,%g4
	ld	[%sp+188],%l3
	srlx	%g3,1,%g3
	xor	%g4,%g2,%g2
	sllx	%g4,7,%g4
	xor	%g3,%g2,%g2
	srlx	%g3,7,%g3
	xor	%g4,%g2,%g2
	sllx	%l6,32,%g5
	xor	%g3,%g2,%g2		! sigma0(X[23+1])
	or	%l7,%g5,%g5

	srlx	%g5,6,%g4
	ld	[%sp+160],%l6
	sllx	%g5,3,%g3
	ld	[%sp+164],%l7
	srlx	%g5,19,%g5
	xor	%g3,%g4,%g4
	sllx	%g3,42,%g3
	xor	%g5,%g4,%g4
	srlx	%g5,42,%g5
	xor	%g3,%g4,%g4
	sllx	%l4,32,%g3
	xor	%g5,%g4,%g4	! sigma1(X[23+14])
	ld	[%sp+120],%l4
	or	%l5,%g3,%g3
	ld	[%sp+124],%l5

	sllx	%l0,32,%g5
	add	%g4,%g2,%g2
	ld	[%sp+176],%l0
	or	%l1,%g5,%g5
	add	%g3,%g2,%g2		! +=X[23+9]
	ld	[%sp+180],%l1
	add	%g5,%g2,%g2		! +=X[23]
	stx	%g2,[%sp+168]
	add	%o0,%g2,%g2
	srlx	%o5,14,%o0	!! 23
	xor	%g1,%o7,%g5
	sllx	%o5,23,%g4
	and	%o5,%g5,%g5
	srlx	%o5,18,%g3
	xor	%g4,%o0,%o0
	sllx	%o5,46,%g4
	xor	%g3,%o0,%o0
	srlx	%o5,41,%g3
	xor	%g4,%o0,%o0
	sllx	%o5,50,%g4
	xor	%g3,%o0,%o0
	xor	%o7,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%o0,%g3		! Sigma1(e)

	srlx	%o1,28,%o0
	add	%g5,%g2,%g2
	ldx	[%i3+184],%g5	! K[23]
	sllx	%o1,25,%g4
	add	%g3,%g2,%g2
	srlx	%o1,34,%g3
	xor	%g4,%o0,%o0
	sllx	%o1,30,%g4
	xor	%g3,%o0,%o0
	srlx	%o1,39,%g3
	xor	%g4,%o0,%o0	
	sllx	%o1,36,%g4
	xor	%g3,%o0,%o0
	xor	%g4,%o0,%o0		! Sigma0(a)

	or	%o1,%o2,%g3
	and	%o1,%o2,%g4
	and	%o3,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[23]
	add	%g4,%o0,%o0

	add	%g2,%o4,%o4
	add	%g2,%o0,%o0
	sllx	%l2,32,%g3		!! Xupdate(24)
	or	%l3,%g3,%g3

	srlx	%g3,7,%g2
	ld	[%sp+192],%l2
	sllx	%g3,56,%g4
	ld	[%sp+196],%l3
	srlx	%g3,1,%g3
	xor	%g4,%g2,%g2
	sllx	%g4,7,%g4
	xor	%g3,%g2,%g2
	srlx	%g3,7,%g3
	xor	%g4,%g2,%g2
	sllx	%l6,32,%g5
	xor	%g3,%g2,%g2		! sigma0(X[24+1])
	or	%l7,%g5,%g5

	srlx	%g5,6,%g4
	ld	[%sp+168],%l6
	sllx	%g5,3,%g3
	ld	[%sp+172],%l7
	srlx	%g5,19,%g5
	xor	%g3,%g4,%g4
	sllx	%g3,42,%g3
	xor	%g5,%g4,%g4
	srlx	%g5,42,%g5
	xor	%g3,%g4,%g4
	sllx	%l4,32,%g3
	xor	%g5,%g4,%g4	! sigma1(X[24+14])
	ld	[%sp+128],%l4
	or	%l5,%g3,%g3
	ld	[%sp+132],%l5

	sllx	%l0,32,%g5
	add	%g4,%g2,%g2
	ld	[%sp+184],%l0
	or	%l1,%g5,%g5
	add	%g3,%g2,%g2		! +=X[24+9]
	ld	[%sp+188],%l1
	add	%g5,%g2,%g2		! +=X[24]
	stx	%g2,[%sp+176]
	add	%o7,%g2,%g2
	srlx	%o4,14,%o7	!! 24
	xor	%o5,%g1,%g5
	sllx	%o4,23,%g4
	and	%o4,%g5,%g5
	srlx	%o4,18,%g3
	xor	%g4,%o7,%o7
	sllx	%o4,46,%g4
	xor	%g3,%o7,%o7
	srlx	%o4,41,%g3
	xor	%g4,%o7,%o7
	sllx	%o4,50,%g4
	xor	%g3,%o7,%o7
	xor	%g1,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%o7,%g3		! Sigma1(e)

	srlx	%o0,28,%o7
	add	%g5,%g2,%g2
	ldx	[%i3+192],%g5	! K[24]
	sllx	%o0,25,%g4
	add	%g3,%g2,%g2
	srlx	%o0,34,%g3
	xor	%g4,%o7,%o7
	sllx	%o0,30,%g4
	xor	%g3,%o7,%o7
	srlx	%o0,39,%g3
	xor	%g4,%o7,%o7	
	sllx	%o0,36,%g4
	xor	%g3,%o7,%o7
	xor	%g4,%o7,%o7		! Sigma0(a)

	or	%o0,%o1,%g3
	and	%o0,%o1,%g4
	and	%o2,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[24]
	add	%g4,%o7,%o7

	add	%g2,%o3,%o3
	add	%g2,%o7,%o7
	sllx	%l2,32,%g3		!! Xupdate(25)
	or	%l3,%g3,%g3

	srlx	%g3,7,%g2
	ld	[%sp+200],%l2
	sllx	%g3,56,%g4
	ld	[%sp+204],%l3
	srlx	%g3,1,%g3
	xor	%g4,%g2,%g2
	sllx	%g4,7,%g4
	xor	%g3,%g2,%g2
	srlx	%g3,7,%g3
	xor	%g4,%g2,%g2
	sllx	%l6,32,%g5
	xor	%g3,%g2,%g2		! sigma0(X[25+1])
	or	%l7,%g5,%g5

	srlx	%g5,6,%g4
	ld	[%sp+176],%l6
	sllx	%g5,3,%g3
	ld	[%sp+180],%l7
	srlx	%g5,19,%g5
	xor	%g3,%g4,%g4
	sllx	%g3,42,%g3
	xor	%g5,%g4,%g4
	srlx	%g5,42,%g5
	xor	%g3,%g4,%g4
	sllx	%l4,32,%g3
	xor	%g5,%g4,%g4	! sigma1(X[25+14])
	ld	[%sp+136],%l4
	or	%l5,%g3,%g3
	ld	[%sp+140],%l5

	sllx	%l0,32,%g5
	add	%g4,%g2,%g2
	ld	[%sp+192],%l0
	or	%l1,%g5,%g5
	add	%g3,%g2,%g2		! +=X[25+9]
	ld	[%sp+196],%l1
	add	%g5,%g2,%g2		! +=X[25]
	stx	%g2,[%sp+184]
	add	%g1,%g2,%g2
	srlx	%o3,14,%g1	!! 25
	xor	%o4,%o5,%g5
	sllx	%o3,23,%g4
	and	%o3,%g5,%g5
	srlx	%o3,18,%g3
	xor	%g4,%g1,%g1
	sllx	%o3,46,%g4
	xor	%g3,%g1,%g1
	srlx	%o3,41,%g3
	xor	%g4,%g1,%g1
	sllx	%o3,50,%g4
	xor	%g3,%g1,%g1
	xor	%o5,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%g1,%g3		! Sigma1(e)

	srlx	%o7,28,%g1
	add	%g5,%g2,%g2
	ldx	[%i3+200],%g5	! K[25]
	sllx	%o7,25,%g4
	add	%g3,%g2,%g2
	srlx	%o7,34,%g3
	xor	%g4,%g1,%g1
	sllx	%o7,30,%g4
	xor	%g3,%g1,%g1
	srlx	%o7,39,%g3
	xor	%g4,%g1,%g1	
	sllx	%o7,36,%g4
	xor	%g3,%g1,%g1
	xor	%g4,%g1,%g1		! Sigma0(a)

	or	%o7,%o0,%g3
	and	%o7,%o0,%g4
	and	%o1,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[25]
	add	%g4,%g1,%g1

	add	%g2,%o2,%o2
	add	%g2,%g1,%g1
	sllx	%l2,32,%g3		!! Xupdate(26)
	or	%l3,%g3,%g3

	srlx	%g3,7,%g2
	ld	[%sp+208],%l2
	sllx	%g3,56,%g4
	ld	[%sp+212],%l3
	srlx	%g3,1,%g3
	xor	%g4,%g2,%g2
	sllx	%g4,7,%g4
	xor	%g3,%g2,%g2
	srlx	%g3,7,%g3
	xor	%g4,%g2,%g2
	sllx	%l6,32,%g5
	xor	%g3,%g2,%g2		! sigma0(X[26+1])
	or	%l7,%g5,%g5

	srlx	%g5,6,%g4
	ld	[%sp+184],%l6
	sllx	%g5,3,%g3
	ld	[%sp+188],%l7
	srlx	%g5,19,%g5
	xor	%g3,%g4,%g4
	sllx	%g3,42,%g3
	xor	%g5,%g4,%g4
	srlx	%g5,42,%g5
	xor	%g3,%g4,%g4
	sllx	%l4,32,%g3
	xor	%g5,%g4,%g4	! sigma1(X[26+14])
	ld	[%sp+144],%l4
	or	%l5,%g3,%g3
	ld	[%sp+148],%l5

	sllx	%l0,32,%g5
	add	%g4,%g2,%g2
	ld	[%sp+200],%l0
	or	%l1,%g5,%g5
	add	%g3,%g2,%g2		! +=X[26+9]
	ld	[%sp+204],%l1
	add	%g5,%g2,%g2		! +=X[26]
	stx	%g2,[%sp+192]
	add	%o5,%g2,%g2
	srlx	%o2,14,%o5	!! 26
	xor	%o3,%o4,%g5
	sllx	%o2,23,%g4
	and	%o2,%g5,%g5
	srlx	%o2,18,%g3
	xor	%g4,%o5,%o5
	sllx	%o2,46,%g4
	xor	%g3,%o5,%o5
	srlx	%o2,41,%g3
	xor	%g4,%o5,%o5
	sllx	%o2,50,%g4
	xor	%g3,%o5,%o5
	xor	%o4,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%o5,%g3		! Sigma1(e)

	srlx	%g1,28,%o5
	add	%g5,%g2,%g2
	ldx	[%i3+208],%g5	! K[26]
	sllx	%g1,25,%g4
	add	%g3,%g2,%g2
	srlx	%g1,34,%g3
	xor	%g4,%o5,%o5
	sllx	%g1,30,%g4
	xor	%g3,%o5,%o5
	srlx	%g1,39,%g3
	xor	%g4,%o5,%o5	
	sllx	%g1,36,%g4
	xor	%g3,%o5,%o5
	xor	%g4,%o5,%o5		! Sigma0(a)

	or	%g1,%o7,%g3
	and	%g1,%o7,%g4
	and	%o0,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[26]
	add	%g4,%o5,%o5

	add	%g2,%o1,%o1
	add	%g2,%o5,%o5
	sllx	%l2,32,%g3		!! Xupdate(27)
	or	%l3,%g3,%g3

	srlx	%g3,7,%g2
	ld	[%sp+216],%l2
	sllx	%g3,56,%g4
	ld	[%sp+220],%l3
	srlx	%g3,1,%g3
	xor	%g4,%g2,%g2
	sllx	%g4,7,%g4
	xor	%g3,%g2,%g2
	srlx	%g3,7,%g3
	xor	%g4,%g2,%g2
	sllx	%l6,32,%g5
	xor	%g3,%g2,%g2		! sigma0(X[27+1])
	or	%l7,%g5,%g5

	srlx	%g5,6,%g4
	ld	[%sp+192],%l6
	sllx	%g5,3,%g3
	ld	[%sp+196],%l7
	srlx	%g5,19,%g5
	xor	%g3,%g4,%g4
	sllx	%g3,42,%g3
	xor	%g5,%g4,%g4
	srlx	%g5,42,%g5
	xor	%g3,%g4,%g4
	sllx	%l4,32,%g3
	xor	%g5,%g4,%g4	! sigma1(X[27+14])
	ld	[%sp+152],%l4
	or	%l5,%g3,%g3
	ld	[%sp+156],%l5

	sllx	%l0,32,%g5
	add	%g4,%g2,%g2
	ld	[%sp+208],%l0
	or	%l1,%g5,%g5
	add	%g3,%g2,%g2		! +=X[27+9]
	ld	[%sp+212],%l1
	add	%g5,%g2,%g2		! +=X[27]
	stx	%g2,[%sp+200]
	add	%o4,%g2,%g2
	srlx	%o1,14,%o4	!! 27
	xor	%o2,%o3,%g5
	sllx	%o1,23,%g4
	and	%o1,%g5,%g5
	srlx	%o1,18,%g3
	xor	%g4,%o4,%o4
	sllx	%o1,46,%g4
	xor	%g3,%o4,%o4
	srlx	%o1,41,%g3
	xor	%g4,%o4,%o4
	sllx	%o1,50,%g4
	xor	%g3,%o4,%o4
	xor	%o3,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%o4,%g3		! Sigma1(e)

	srlx	%o5,28,%o4
	add	%g5,%g2,%g2
	ldx	[%i3+216],%g5	! K[27]
	sllx	%o5,25,%g4
	add	%g3,%g2,%g2
	srlx	%o5,34,%g3
	xor	%g4,%o4,%o4
	sllx	%o5,30,%g4
	xor	%g3,%o4,%o4
	srlx	%o5,39,%g3
	xor	%g4,%o4,%o4	
	sllx	%o5,36,%g4
	xor	%g3,%o4,%o4
	xor	%g4,%o4,%o4		! Sigma0(a)

	or	%o5,%g1,%g3
	and	%o5,%g1,%g4
	and	%o7,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[27]
	add	%g4,%o4,%o4

	add	%g2,%o0,%o0
	add	%g2,%o4,%o4
	sllx	%l2,32,%g3		!! Xupdate(28)
	or	%l3,%g3,%g3

	srlx	%g3,7,%g2
	ld	[%sp+224],%l2
	sllx	%g3,56,%g4
	ld	[%sp+228],%l3
	srlx	%g3,1,%g3
	xor	%g4,%g2,%g2
	sllx	%g4,7,%g4
	xor	%g3,%g2,%g2
	srlx	%g3,7,%g3
	xor	%g4,%g2,%g2
	sllx	%l6,32,%g5
	xor	%g3,%g2,%g2		! sigma0(X[28+1])
	or	%l7,%g5,%g5

	srlx	%g5,6,%g4
	ld	[%sp+200],%l6
	sllx	%g5,3,%g3
	ld	[%sp+204],%l7
	srlx	%g5,19,%g5
	xor	%g3,%g4,%g4
	sllx	%g3,42,%g3
	xor	%g5,%g4,%g4
	srlx	%g5,42,%g5
	xor	%g3,%g4,%g4
	sllx	%l4,32,%g3
	xor	%g5,%g4,%g4	! sigma1(X[28+14])
	ld	[%sp+160],%l4
	or	%l5,%g3,%g3
	ld	[%sp+164],%l5

	sllx	%l0,32,%g5
	add	%g4,%g2,%g2
	ld	[%sp+216],%l0
	or	%l1,%g5,%g5
	add	%g3,%g2,%g2		! +=X[28+9]
	ld	[%sp+220],%l1
	add	%g5,%g2,%g2		! +=X[28]
	stx	%g2,[%sp+208]
	add	%o3,%g2,%g2
	srlx	%o0,14,%o3	!! 28
	xor	%o1,%o2,%g5
	sllx	%o0,23,%g4
	and	%o0,%g5,%g5
	srlx	%o0,18,%g3
	xor	%g4,%o3,%o3
	sllx	%o0,46,%g4
	xor	%g3,%o3,%o3
	srlx	%o0,41,%g3
	xor	%g4,%o3,%o3
	sllx	%o0,50,%g4
	xor	%g3,%o3,%o3
	xor	%o2,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%o3,%g3		! Sigma1(e)

	srlx	%o4,28,%o3
	add	%g5,%g2,%g2
	ldx	[%i3+224],%g5	! K[28]
	sllx	%o4,25,%g4
	add	%g3,%g2,%g2
	srlx	%o4,34,%g3
	xor	%g4,%o3,%o3
	sllx	%o4,30,%g4
	xor	%g3,%o3,%o3
	srlx	%o4,39,%g3
	xor	%g4,%o3,%o3	
	sllx	%o4,36,%g4
	xor	%g3,%o3,%o3
	xor	%g4,%o3,%o3		! Sigma0(a)

	or	%o4,%o5,%g3
	and	%o4,%o5,%g4
	and	%g1,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[28]
	add	%g4,%o3,%o3

	add	%g2,%o7,%o7
	add	%g2,%o3,%o3
	sllx	%l2,32,%g3		!! Xupdate(29)
	or	%l3,%g3,%g3

	srlx	%g3,7,%g2
	ld	[%sp+232],%l2
	sllx	%g3,56,%g4
	ld	[%sp+236],%l3
	srlx	%g3,1,%g3
	xor	%g4,%g2,%g2
	sllx	%g4,7,%g4
	xor	%g3,%g2,%g2
	srlx	%g3,7,%g3
	xor	%g4,%g2,%g2
	sllx	%l6,32,%g5
	xor	%g3,%g2,%g2		! sigma0(X[29+1])
	or	%l7,%g5,%g5

	srlx	%g5,6,%g4
	ld	[%sp+208],%l6
	sllx	%g5,3,%g3
	ld	[%sp+212],%l7
	srlx	%g5,19,%g5
	xor	%g3,%g4,%g4
	sllx	%g3,42,%g3
	xor	%g5,%g4,%g4
	srlx	%g5,42,%g5
	xor	%g3,%g4,%g4
	sllx	%l4,32,%g3
	xor	%g5,%g4,%g4	! sigma1(X[29+14])
	ld	[%sp+168],%l4
	or	%l5,%g3,%g3
	ld	[%sp+172],%l5

	sllx	%l0,32,%g5
	add	%g4,%g2,%g2
	ld	[%sp+224],%l0
	or	%l1,%g5,%g5
	add	%g3,%g2,%g2		! +=X[29+9]
	ld	[%sp+228],%l1
	add	%g5,%g2,%g2		! +=X[29]
	stx	%g2,[%sp+216]
	add	%o2,%g2,%g2
	srlx	%o7,14,%o2	!! 29
	xor	%o0,%o1,%g5
	sllx	%o7,23,%g4
	and	%o7,%g5,%g5
	srlx	%o7,18,%g3
	xor	%g4,%o2,%o2
	sllx	%o7,46,%g4
	xor	%g3,%o2,%o2
	srlx	%o7,41,%g3
	xor	%g4,%o2,%o2
	sllx	%o7,50,%g4
	xor	%g3,%o2,%o2
	xor	%o1,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%o2,%g3		! Sigma1(e)

	srlx	%o3,28,%o2
	add	%g5,%g2,%g2
	ldx	[%i3+232],%g5	! K[29]
	sllx	%o3,25,%g4
	add	%g3,%g2,%g2
	srlx	%o3,34,%g3
	xor	%g4,%o2,%o2
	sllx	%o3,30,%g4
	xor	%g3,%o2,%o2
	srlx	%o3,39,%g3
	xor	%g4,%o2,%o2	
	sllx	%o3,36,%g4
	xor	%g3,%o2,%o2
	xor	%g4,%o2,%o2		! Sigma0(a)

	or	%o3,%o4,%g3
	and	%o3,%o4,%g4
	and	%o5,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[29]
	add	%g4,%o2,%o2

	add	%g2,%g1,%g1
	add	%g2,%o2,%o2
	sllx	%l2,32,%g3		!! Xupdate(30)
	or	%l3,%g3,%g3

	srlx	%g3,7,%g2
	ld	[%sp+112],%l2
	sllx	%g3,56,%g4
	ld	[%sp+116],%l3
	srlx	%g3,1,%g3
	xor	%g4,%g2,%g2
	sllx	%g4,7,%g4
	xor	%g3,%g2,%g2
	srlx	%g3,7,%g3
	xor	%g4,%g2,%g2
	sllx	%l6,32,%g5
	xor	%g3,%g2,%g2		! sigma0(X[30+1])
	or	%l7,%g5,%g5

	srlx	%g5,6,%g4
	ld	[%sp+216],%l6
	sllx	%g5,3,%g3
	ld	[%sp+220],%l7
	srlx	%g5,19,%g5
	xor	%g3,%g4,%g4
	sllx	%g3,42,%g3
	xor	%g5,%g4,%g4
	srlx	%g5,42,%g5
	xor	%g3,%g4,%g4
	sllx	%l4,32,%g3
	xor	%g5,%g4,%g4	! sigma1(X[30+14])
	ld	[%sp+176],%l4
	or	%l5,%g3,%g3
	ld	[%sp+180],%l5

	sllx	%l0,32,%g5
	add	%g4,%g2,%g2
	ld	[%sp+232],%l0
	or	%l1,%g5,%g5
	add	%g3,%g2,%g2		! +=X[30+9]
	ld	[%sp+236],%l1
	add	%g5,%g2,%g2		! +=X[30]
	stx	%g2,[%sp+224]
	add	%o1,%g2,%g2
	srlx	%g1,14,%o1	!! 30
	xor	%o7,%o0,%g5
	sllx	%g1,23,%g4
	and	%g1,%g5,%g5
	srlx	%g1,18,%g3
	xor	%g4,%o1,%o1
	sllx	%g1,46,%g4
	xor	%g3,%o1,%o1
	srlx	%g1,41,%g3
	xor	%g4,%o1,%o1
	sllx	%g1,50,%g4
	xor	%g3,%o1,%o1
	xor	%o0,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%o1,%g3		! Sigma1(e)

	srlx	%o2,28,%o1
	add	%g5,%g2,%g2
	ldx	[%i3+240],%g5	! K[30]
	sllx	%o2,25,%g4
	add	%g3,%g2,%g2
	srlx	%o2,34,%g3
	xor	%g4,%o1,%o1
	sllx	%o2,30,%g4
	xor	%g3,%o1,%o1
	srlx	%o2,39,%g3
	xor	%g4,%o1,%o1	
	sllx	%o2,36,%g4
	xor	%g3,%o1,%o1
	xor	%g4,%o1,%o1		! Sigma0(a)

	or	%o2,%o3,%g3
	and	%o2,%o3,%g4
	and	%o4,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[30]
	add	%g4,%o1,%o1

	add	%g2,%o5,%o5
	add	%g2,%o1,%o1
	sllx	%l2,32,%g3		!! Xupdate(31)
	or	%l3,%g3,%g3

	srlx	%g3,7,%g2
	ld	[%sp+120],%l2
	sllx	%g3,56,%g4
	ld	[%sp+124],%l3
	srlx	%g3,1,%g3
	xor	%g4,%g2,%g2
	sllx	%g4,7,%g4
	xor	%g3,%g2,%g2
	srlx	%g3,7,%g3
	xor	%g4,%g2,%g2
	sllx	%l6,32,%g5
	xor	%g3,%g2,%g2		! sigma0(X[31+1])
	or	%l7,%g5,%g5

	srlx	%g5,6,%g4
	ld	[%sp+224],%l6
	sllx	%g5,3,%g3
	ld	[%sp+228],%l7
	srlx	%g5,19,%g5
	xor	%g3,%g4,%g4
	sllx	%g3,42,%g3
	xor	%g5,%g4,%g4
	srlx	%g5,42,%g5
	xor	%g3,%g4,%g4
	sllx	%l4,32,%g3
	xor	%g5,%g4,%g4	! sigma1(X[31+14])
	ld	[%sp+184],%l4
	or	%l5,%g3,%g3
	ld	[%sp+188],%l5

	sllx	%l0,32,%g5
	add	%g4,%g2,%g2
	ld	[%sp+112],%l0
	or	%l1,%g5,%g5
	add	%g3,%g2,%g2		! +=X[31+9]
	ld	[%sp+116],%l1
	add	%g5,%g2,%g2		! +=X[31]
	stx	%g2,[%sp+232]
	add	%o0,%g2,%g2
	srlx	%o5,14,%o0	!! 31
	xor	%g1,%o7,%g5
	sllx	%o5,23,%g4
	and	%o5,%g5,%g5
	srlx	%o5,18,%g3
	xor	%g4,%o0,%o0
	sllx	%o5,46,%g4
	xor	%g3,%o0,%o0
	srlx	%o5,41,%g3
	xor	%g4,%o0,%o0
	sllx	%o5,50,%g4
	xor	%g3,%o0,%o0
	xor	%o7,%g5,%g5		! Ch(e,f,g)
	xor	%g4,%o0,%g3		! Sigma1(e)

	srlx	%o1,28,%o0
	add	%g5,%g2,%g2
	ldx	[%i3+248],%g5	! K[31]
	sllx	%o1,25,%g4
	add	%g3,%g2,%g2
	srlx	%o1,34,%g3
	xor	%g4,%o0,%o0
	sllx	%o1,30,%g4
	xor	%g3,%o0,%o0
	srlx	%o1,39,%g3
	xor	%g4,%o0,%o0	
	sllx	%o1,36,%g4
	xor	%g3,%o0,%o0
	xor	%g4,%o0,%o0		! Sigma0(a)

	or	%o1,%o2,%g3
	and	%o1,%o2,%g4
	and	%o3,%g3,%g3
	or	%g3,%g4,%g4	! Maj(a,b,c)
	add	%g5,%g2,%g2		! +=K[31]
	add	%g4,%o0,%o0

	add	%g2,%o4,%o4
	add	%g2,%o0,%o0
	and	%g5,0xfff,%g5
	cmp	%g5,2071
	bne	.L16_xx
	add	%i3,128,%i3	! Ktbl+=16

	ld	[%i0+0],%l0
	ld	[%i0+4],%l1
	ld	[%i0+8],%l2
	ld	[%i0+12],%l3
	ld	[%i0+16],%l4
	ld	[%i0+20],%l5
	ld	[%i0+24],%l6

	sllx	%l0,32,%g3
	ld	[%i0+28],%l7
	sllx	%l2,32,%g4
	or	%l1,%g3,%g3
	or	%l3,%g4,%g4
	add	%g3,%o0,%o0
	add	%g4,%o1,%o1
	stx	%o0,[%i0+0]
	sllx	%l4,32,%g5
	stx	%o1,[%i0+8]
	sllx	%l6,32,%g2
	or	%l5,%g5,%g5
	or	%l7,%g2,%g2
	add	%g5,%o2,%o2
	stx	%o2,[%i0+16]
	add	%g2,%o3,%o3
	stx	%o3,[%i0+24]

	ld	[%i0+32],%l0
	ld	[%i0+36],%l1
	ld	[%i0+40],%l2
	ld	[%i0+44],%l3
	ld	[%i0+48],%l4
	ld	[%i0+52],%l5
	ld	[%i0+56],%l6

	sllx	%l0,32,%g3
	ld	[%i0+60],%l7
	sllx	%l2,32,%g4
	or	%l1,%g3,%g3
	or	%l3,%g4,%g4
	add	%g3,%o4,%o4
	add	%g4,%o5,%o5
	stx	%o4,[%i0+32]
	sllx	%l4,32,%g5
	stx	%o5,[%i0+40]
	sllx	%l6,32,%g2
	or	%l5,%g5,%g5
	or	%l7,%g2,%g2
	add	%g5,%g1,%g1
	stx	%g1,[%i0+48]
	add	%g2,%o7,%o7
	stx	%o7,[%i0+56]
	add	%i1,128,%i1		! advance inp
	cmp	%i1,%i2
	bne	%icc,.Lloop
	sub	%i3,512,%i3	! rewind Ktbl

	ret
	restore
.type	sha512_block_data_order,#function
.size	sha512_block_data_order,(.-sha512_block_data_order)
.asciz	"SHA512 block transform for SPARCv9, CRYPTOGAMS by <appro@openssl.org>"
.align	4
