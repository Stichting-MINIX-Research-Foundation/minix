.machine	"any"

.text

.globl	gcm_init_p8
.type	gcm_init_p8,@function
.align	5
gcm_init_p8:
	lis	0,0xfff0
	li	8,0x10
	mfspr	12,256
	li	9,0x20
	mtspr	256,0
	li	10,0x30
.long	0x7D202699

	vspltisb	8,-16
	vspltisb	5,1
	vaddubm	8,8,8
	vxor	4,4,4
	vor	8,8,5
	vsldoi	8,8,4,15
	vsldoi	6,4,5,1
	vaddubm	8,8,8
	vspltisb	7,7
	vor	8,8,6
	vspltb	6,9,0
	vsl	9,9,5
	vsrab	6,6,7
	vand	6,6,8
	vxor	9,9,6

	vsldoi	9,9,9,8
	vsldoi	8,4,8,8
	vsldoi	11,4,9,8
	vsldoi	10,9,4,8

.long	0x7D001F99
.long	0x7D681F99
.long	0x7D291F99
.long	0x7D4A1F99

	mtspr	256,12
	blr	
.long	0
.byte	0,12,0x14,0,0,0,2,0
.long	0


.globl	gcm_gmult_p8
.type	gcm_gmult_p8,@function
.align	5
gcm_gmult_p8:
	lis	0,0xfff8
	li	8,0x10
	mfspr	12,256
	li	9,0x20
	mtspr	256,0
	li	10,0x30
.long	0x7C601E99

.long	0x7D682699

.long	0x7D292699

.long	0x7D4A2699

.long	0x7D002699

	vxor	4,4,4

.long	0x10035CC8
.long	0x10234CC8
.long	0x104354C8

.long	0x10E044C8

	vsldoi	5,1,4,8
	vsldoi	6,4,1,8
	vxor	0,0,5
	vxor	2,2,6

	vsldoi	0,0,0,8
	vxor	0,0,7

	vsldoi	6,0,0,8
.long	0x100044C8
	vxor	6,6,2
	vxor	0,0,6


.long	0x7C001F99

	mtspr	256,12
	blr	
.long	0
.byte	0,12,0x14,0,0,0,2,0
.long	0


.globl	gcm_ghash_p8
.type	gcm_ghash_p8,@function
.align	5
gcm_ghash_p8:
	lis	0,0xfff8
	li	8,0x10
	mfspr	12,256
	li	9,0x20
	mtspr	256,0
	li	10,0x30
.long	0x7C001E99

.long	0x7D682699

.long	0x7D292699

.long	0x7D4A2699

.long	0x7D002699

	vxor	4,4,4

.long	0x7C602E99
	addi	5,5,16
	subi	6,6,16

	vxor	3,3,0
	b	.Loop

.align	5
.Loop:
	subic	6,6,16
.long	0x10035CC8
	subfe.	0,0,0
.long	0x10234CC8
	and	0,0,6
.long	0x104354C8
	add	5,5,0

.long	0x10E044C8

	vsldoi	5,1,4,8
	vsldoi	6,4,1,8
	vxor	0,0,5
	vxor	2,2,6

	vsldoi	0,0,0,8
	vxor	0,0,7
.long	0x7C602E99
	addi	5,5,16

	vsldoi	6,0,0,8
.long	0x100044C8

	vxor	6,6,2
	vxor	3,3,6
	vxor	3,3,0
	beq	.Loop

	vxor	0,0,6

.long	0x7C001F99

	mtspr	256,12
	blr	
.long	0
.byte	0,12,0x14,0,0,0,4,0
.long	0


.byte	71,72,65,83,72,32,102,111,114,32,80,111,119,101,114,73,83,65,32,50,46,48,55,44,32,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.align	2
.align	2
