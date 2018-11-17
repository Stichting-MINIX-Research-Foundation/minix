.machine	"any"
.text

.globl	OPENSSL_ppc64_probe
.type	OPENSSL_ppc64_probe,@function
.section	".opd","aw"
.align	3
OPENSSL_ppc64_probe:
.quad	.OPENSSL_ppc64_probe,.TOC.@tocbase,0
.previous

.align	4
.OPENSSL_ppc64_probe:
	fcfid	1,1
	rldicl	0,0,32,32
	blr	
.long	0
.byte	0,12,0x14,0,0,0,0,0


.globl	OPENSSL_altivec_probe
.type	OPENSSL_altivec_probe,@function
.section	".opd","aw"
.align	3
OPENSSL_altivec_probe:
.quad	.OPENSSL_altivec_probe,.TOC.@tocbase,0
.previous

.align	4
.OPENSSL_altivec_probe:
.long	0x10000484
	blr	
.long	0
.byte	0,12,0x14,0,0,0,0,0


.globl	OPENSSL_crypto207_probe
.type	OPENSSL_crypto207_probe,@function
.section	".opd","aw"
.align	3
OPENSSL_crypto207_probe:
.quad	.OPENSSL_crypto207_probe,.TOC.@tocbase,0
.previous

.align	4
.OPENSSL_crypto207_probe:
.long	0x7C000E99
.long	0x10000508
	blr	
.long	0
.byte	0,12,0x14,0,0,0,0,0


.globl	OPENSSL_wipe_cpu
.type	OPENSSL_wipe_cpu,@function
.section	".opd","aw"
.align	3
OPENSSL_wipe_cpu:
.quad	.OPENSSL_wipe_cpu,.TOC.@tocbase,0
.previous

.align	4
.OPENSSL_wipe_cpu:
	xor	0,0,0
	fmr	0,31
	fmr	1,31
	fmr	2,31
	mr	3,1
	fmr	3,31
	xor	4,4,4
	fmr	4,31
	xor	5,5,5
	fmr	5,31
	xor	6,6,6
	fmr	6,31
	xor	7,7,7
	fmr	7,31
	xor	8,8,8
	fmr	8,31
	xor	9,9,9
	fmr	9,31
	xor	10,10,10
	fmr	10,31
	xor	11,11,11
	fmr	11,31
	xor	12,12,12
	fmr	12,31
	fmr	13,31
	blr	
.long	0
.byte	0,12,0x14,0,0,0,0,0


.globl	OPENSSL_atomic_add
.type	OPENSSL_atomic_add,@function
.section	".opd","aw"
.align	3
OPENSSL_atomic_add:
.quad	.OPENSSL_atomic_add,.TOC.@tocbase,0
.previous

.align	4
.OPENSSL_atomic_add:
.Ladd:	lwarx	5,0,3
	add	0,4,5
	stwcx.	0,0,3
	bne-	.Ladd
	extsw	3,0
	blr	
.long	0
.byte	0,12,0x14,0,0,0,2,0
.long	0


.globl	OPENSSL_rdtsc
.type	OPENSSL_rdtsc,@function
.section	".opd","aw"
.align	3
OPENSSL_rdtsc:
.quad	.OPENSSL_rdtsc,.TOC.@tocbase,0
.previous

.align	4
.OPENSSL_rdtsc:
.Lrdtsc:
	mftb	5
	mftbu	4
	mftb	3
	cmplw	0,3,5
	bne	.Lrdtsc
	blr	
.long	0
.byte	0,12,0x14,0,0,0,0,0


.globl	OPENSSL_cleanse
.type	OPENSSL_cleanse,@function
.section	".opd","aw"
.align	3
OPENSSL_cleanse:
.quad	.OPENSSL_cleanse,.TOC.@tocbase,0
.previous

.align	4
.OPENSSL_cleanse:
	cmpldi	4,7
	li	0,0
	bge	.Lot
	cmpldi	4,0
	beqlr
.Little:	mtctr	4
	stb	0,0(3)
	addi	3,3,1
	bc	16,0,$-8
	blr	
.Lot:	andi.	5,3,3
	beq	.Laligned
	stb	0,0(3)
	subi	4,4,1
	addi	3,3,1
	b	.Lot
.Laligned:
	srdi	5,4,2
	mtctr	5
	stw	0,0(3)
	addi	3,3,4
	bc	16,0,$-8
	andi.	4,4,3
	bne	.Little
	blr	
.long	0
.byte	0,12,0x14,0,0,0,2,0
.long	0

