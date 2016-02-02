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
.asciz	"GHASH for SPARCv9, CRYPTOGAMS by <appro@openssl.org>"
.align	4
