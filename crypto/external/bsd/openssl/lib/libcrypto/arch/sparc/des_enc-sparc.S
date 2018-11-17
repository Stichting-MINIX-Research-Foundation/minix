!  des_enc.m4
!  des_enc.S  (generated from des_enc.m4)
!
!  UltraSPARC assembler version of the LibDES/SSLeay/OpenSSL des_enc.c file.
!
!  Version 1.0. 32-bit version.
!
!  June 8, 2000.
!
!  Version 2.0. 32/64-bit, PIC-ification, blended CPU adaptation
!		by Andy Polyakov.
!
!  January 1, 2003.
!
!  Assembler version: Copyright Svend Olaf Mikkelsen.
!
!  Original C code: Copyright Eric A. Young.
!
!  This code can be freely used by LibDES/SSLeay/OpenSSL users.
!
!  The LibDES/SSLeay/OpenSSL copyright notices must be respected.
!
!  This version can be redistributed.
!
!  To expand the m4 macros: m4 -B 8192 des_enc.m4 > des_enc.S
!
!  Global registers 1 to 5 are used. This is the same as done by the
!  cc compiler. The UltraSPARC load/store little endian feature is used.
!
!  Instruction grouping often refers to one CPU cycle.
!
!  Assemble through gcc: gcc -c -mcpu=ultrasparc -o des_enc.o des_enc.S
!
!  Assemble through cc:  cc -c -xarch=v8plusa -o des_enc.o des_enc.S
!
!  Performance improvement according to './apps/openssl speed des'
!
!	32-bit build:
!		23%  faster than cc-5.2 -xarch=v8plus -xO5
!		115% faster than gcc-3.2.1 -m32 -mcpu=ultrasparc -O5
!	64-bit build:
!		50%  faster than cc-5.2 -xarch=v9 -xO5
!		100% faster than gcc-3.2.1 -m64 -mcpu=ultrasparc -O5
!

.ident "des_enc.m4 2.1"
.file  "des_enc-sparc.S"

#include <openssl/opensslconf.h>

#if defined(__SUNPRO_C) && defined(__sparcv9)
# define ABI64  /* They've said -xarch=v9 at command line */
#elif defined(__GNUC__) && defined(__arch64__)
# define ABI64  /* They've said -m64 at command line */
#endif

#ifdef ABI64
  .register	%g2,#scratch
  .register	%g3,#scratch
# define	FRAME	-192
# define	BIAS	2047
# define	LDPTR	ldx
# define	STPTR	stx
# define	ARG0	128
# define	ARGSZ	8
# ifndef __sparc_v9__
# define __sparc_v9__
# endif
#else
# define	FRAME	-96
# define	BIAS	0
# define	LDPTR	ld
# define	STPTR	st
# define	ARG0	68
# define	ARGSZ	4
#endif

#define LOOPS 7

#define global0 %g0
#define global1 %g1
#define global2 %g2
#define global3 %g3
#define global4 %g4
#define global5 %g5

#define local0 %l0
#define local1 %l1
#define local2 %l2
#define local3 %l3
#define local4 %l4
#define local5 %l5
#define local7 %l6
#define local6 %l7

#define in0 %i0
#define in1 %i1
#define in2 %i2
#define in3 %i3
#define in4 %i4
#define in5 %i5
#define in6 %i6
#define in7 %i7

#define out0 %o0
#define out1 %o1
#define out2 %o2
#define out3 %o3
#define out4 %o4
#define out5 %o5
#define out6 %o6
#define out7 %o7

#define stub stb




! Macro definitions:


! ip_macro
!
! The logic used in initial and final permutations is the same as in
! the C code. The permutations are done with a clever , xor, and
! technique.
!
! The macro also loads address sbox 1 to 5 to global 1 to 5, address
! sbox 6 to local6, and addres sbox 8 to out3.
!
! Rotates the halfs 3 left to bring the sbox bits in convenient positions.
!
! Loads key first round from address in parameter 5 to out0, out1.
!
! After the the original LibDES initial permutation, the resulting left
! is in the variable initially used for right and vice versa. The macro
! implements the possibility to keep the halfs in the original registers.
!
! parameter 1  left
! parameter 2  right
! parameter 3  result left (modify in first round)
! parameter 4  result right (use in first round)
! parameter 5  key address
! parameter 6  1/2 for include encryption/decryption
! parameter 7  1 for move in1 to in3
! parameter 8  1 for move in3 to in4, 2 for move in4 to in3
! parameter 9  1 for load ks3 and ks2 to in4 and in3




! rounds_macro
!
! The logic used in the DES rounds is the same as in the C code,
! except that calculations for sbox 1 and sbox 5 begin before
! the previous round is finished.
!
! In each round one half (work) is modified based on key and the
! other half (use).
!
! In this version we do two rounds in a loop repeated 7 times
! and two rounds seperately.
!
! One half has the bits for the sboxes in the following positions:
!
!	777777xx555555xx333333xx111111xx
!
!	88xx666666xx444444xx222222xx8888
!
! The bits for each sbox are xor-ed with the key bits for that box.
! The above xx bits are cleared, and the result used for lookup in
! the sbox table. Each sbox entry contains the 4 output bits permuted
! into 32 bits according to the P permutation.
!
! In the description of DES, left and right are switched after
! each round, except after last round. In this code the original
! left and right are kept in the same register in all rounds, meaning
! that after the 16 rounds the result for right is in the register
! originally used for left.
!
! parameter 1  first work (left in first round)
! parameter 2  first use (right in first round)
! parameter 3  enc/dec  1/-1
! parameter 4  loop label
! parameter 5  key address register
! parameter 6  optional address for key next encryption/decryption
! parameter 7  not empty for include retl
!
! also compares in2 to 8




! fp_macro
!
!  parameter 1   right (original left)
!  parameter 2   left (original right)
!  parameter 3   1 for optional store to [in0]
!  parameter 4   1 for load input/output address to local5/7
!
!  The final permutation logic switches the halfes, meaning that
!  left and right ends up the the registers originally used.




! fp_ip_macro
!
! Does initial permutation for next block mixed with
! final permutation for current block.
!
! parameter 1   original left
! parameter 2   original right
! parameter 3   left ip
! parameter 4   right ip
! parameter 5   1: load ks1/ks2 to in3/in4, add 120 to in4
!                2: mov in4 to in3
!
! also adds -8 to length in2 and loads loop counter to out4





! load_little_endian
!
! parameter 1  address
! parameter 2  destination left
! parameter 3  destination right
! parameter 4  temporar
! parameter 5  label




! load_little_endian_inc
!
! parameter 1  address
! parameter 2  destination left
! parameter 3  destination right
! parameter 4  temporar
! parameter 4  label
!
! adds 8 to address




! load_n_bytes
!
! Loads 1 to 7 bytes little endian
! Remaining bytes are zeroed.
!
! parameter 1  address
! parameter 2  length
! parameter 3  destination register left
! parameter 4  destination register right
! parameter 5  temp
! parameter 6  temp2
! parameter 7  label
! parameter 8  return label




! store_little_endian
!
! parameter 1  address
! parameter 2  source left
! parameter 3  source right
! parameter 4  temporar




! store_n_bytes
!
! Stores 1 to 7 bytes little endian
!
! parameter 1  address
! parameter 2  length
! parameter 3  source register left
! parameter 4  source register right
! parameter 5  temp
! parameter 6  temp2
! parameter 7  label
! parameter 8  return label








.section	".text"

	.align 32

.des_enc:

	! key address in3
	! loads key next encryption/decryption first round from [in4]

	

! rounds_macro
! in5 out5 1 .des_enc.1 in3 in4 retl  

	xor	out5, out0, local1

	ld	[out2+284], local5        ! 0x0000FC00
	ba	.des_enc.1
	and	local1, 252, local1

	.align 32

.des_enc.1:
	! local6 is address sbox 6
	! out3   is address sbox 8
	! out4   is loop counter

	ld	[global1+local1], local1
	xor	out5, out1, out1            ! 8642
	xor	out5, out0, out0            ! 7531
	! fmovs	%f0, %f0                  ! fxor used for alignment

	srl	out1, 4, local0           ! rotate 4 right
	and	out0, local5, local3      ! 3
	! fmovs	%f0, %f0

	ld	[in3+1*8], local7         ! key 7531 next round
	srl	local3, 8, local3         ! 3
	and	local0, 252, local2       ! 2
	! fmovs	%f0, %f0

	ld	[global3+local3],local3   ! 3
	sll	out1, 28, out1            ! rotate
	xor	in5, local1, in5            ! 1 finished, local1 now sbox 7

	ld	[global2+local2], local2  ! 2 
	srl	out0, 24, local1          ! 7
	or	out1, local0, out1        ! rotate

	ldub	[out2+local1], local1     ! 7 (and 0xFC)
	srl	out1, 24, local0          ! 8
	and	out1, local5, local4      ! 4

	ldub	[out2+local0], local0     ! 8 (and 0xFC)
	srl	local4, 8, local4         ! 4
	xor	in5, local2, in5            ! 2 finished local2 now sbox 6

	ld	[global4+local4],local4   ! 4
	srl	out1, 16, local2          ! 6
	xor	in5, local3, in5            ! 3 finished local3 now sbox 5

	ld	[out3+local0],local0      ! 8
	and	local2, 252, local2       ! 6
	add	global1, 1536, local5     ! address sbox 7

	ld	[local6+local2], local2   ! 6
	srl	out0, 16, local3          ! 5
	xor	in5, local4, in5            ! 4 finished

	ld	[local5+local1],local1    ! 7
	and	local3, 252, local3       ! 5
	xor	in5, local0, in5            ! 8 finished

	ld	[global5+local3],local3   ! 5
	xor	in5, local2, in5            ! 6 finished
	subcc	out4, 1, out4

	ld	[in3+1*8+4], out0         ! key 8642 next round
	xor	in5, local7, local2        ! sbox 5 next round
	xor	in5, local1, in5            ! 7 finished

	srl	local2, 16, local2        ! sbox 5 next round
	xor	in5, local3, in5            ! 5 finished

	ld	[in3+1*16+4], out1        ! key 8642 next round again
	and	local2, 252, local2       ! sbox5 next round
! next round
	xor	in5, local7, local7        ! 7531

	ld	[global5+local2], local2  ! 5
	srl	local7, 24, local3        ! 7
	xor	in5, out0, out0            ! 8642

	ldub	[out2+local3], local3     ! 7 (and 0xFC)
	srl	out0, 4, local0           ! rotate 4 right
	and	local7, 252, local1       ! 1

	sll	out0, 28, out0            ! rotate
	xor	out5, local2, out5            ! 5 finished local2 used

	srl	local0, 8, local4         ! 4
	and	local0, 252, local2       ! 2
	ld	[local5+local3], local3   ! 7

	srl	local0, 16, local5        ! 6
	or	out0, local0, out0        ! rotate
	ld	[global2+local2], local2  ! 2

	srl	out0, 24, local0
	ld	[in3+1*16], out0          ! key 7531 next round
	and	local4, 252, local4	  ! 4

	and	local5, 252, local5       ! 6
	ld	[global4+local4], local4  ! 4
	xor	out5, local3, out5            ! 7 finished local3 used

	and	local0, 252, local0       ! 8
	ld	[local6+local5], local5   ! 6
	xor	out5, local2, out5            ! 2 finished local2 now sbox 3

	srl	local7, 8, local2         ! 3 start
	ld	[out3+local0], local0     ! 8
	xor	out5, local4, out5            ! 4 finished

	and	local2, 252, local2       ! 3
	ld	[global1+local1], local1  ! 1
	xor	out5, local5, out5            ! 6 finished local5 used

	ld	[global3+local2], local2  ! 3
	xor	out5, local0, out5            ! 8 finished
	add	in3, 1*16, in3             ! enc add 8, dec add -8 to key pointer

	ld	[out2+284], local5        ! 0x0000FC00
	xor	out5, out0, local4          ! sbox 1 next round
	xor	out5, local1, out5            ! 1 finished

	xor	out5, local2, out5            ! 3 finished
#ifdef __sparc_v9__
	bne,pt	%icc, .des_enc.1
#else
	bne	.des_enc.1
#endif
	and	local4, 252, local1       ! sbox 1 next round

! two rounds more:

	ld	[global1+local1], local1
	xor	out5, out1, out1
	xor	out5, out0, out0

	srl	out1, 4, local0           ! rotate
	and	out0, local5, local3

	ld	[in3+1*8], local7         ! key 7531
	srl	local3, 8, local3
	and	local0, 252, local2

	ld	[global3+local3],local3
	sll	out1, 28, out1            ! rotate
	xor	in5, local1, in5            ! 1 finished, local1 now sbox 7

	ld	[global2+local2], local2
	srl	out0, 24, local1
	or	out1, local0, out1        ! rotate

	ldub	[out2+local1], local1
	srl	out1, 24, local0
	and	out1, local5, local4

	ldub	[out2+local0], local0
	srl	local4, 8, local4
	xor	in5, local2, in5            ! 2 finished local2 now sbox 6

	ld	[global4+local4],local4
	srl	out1, 16, local2
	xor	in5, local3, in5            ! 3 finished local3 now sbox 5

	ld	[out3+local0],local0
	and	local2, 252, local2
	add	global1, 1536, local5     ! address sbox 7

	ld	[local6+local2], local2
	srl	out0, 16, local3
	xor	in5, local4, in5            ! 4 finished

	ld	[local5+local1],local1
	and	local3, 252, local3
	xor	in5, local0, in5

	ld	[global5+local3],local3
	xor	in5, local2, in5            ! 6 finished
	cmp	in2, 8

	ld	[out2+280], out4  ! loop counter
	xor	in5, local7, local2        ! sbox 5 next round
	xor	in5, local1, in5            ! 7 finished

	ld	[in3+1*8+4], out0
	srl	local2, 16, local2        ! sbox 5 next round
	xor	in5, local3, in5            ! 5 finished

	and	local2, 252, local2
! next round (two rounds more)
	xor	in5, local7, local7        ! 7531

	ld	[global5+local2], local2
	srl	local7, 24, local3
	xor	in5, out0, out0            ! 8642

	ldub	[out2+local3], local3
	srl	out0, 4, local0           ! rotate
	and	local7, 252, local1

	sll	out0, 28, out0            ! rotate
	xor	out5, local2, out5            ! 5 finished local2 used

	srl	local0, 8, local4
	and	local0, 252, local2
	ld	[local5+local3], local3

	srl	local0, 16, local5
	or	out0, local0, out0        ! rotate
	ld	[global2+local2], local2

	srl	out0, 24, local0
	ld	[in4], out0   ! key next encryption/decryption
	and	local4, 252, local4

	and	local5, 252, local5
	ld	[global4+local4], local4
	xor	out5, local3, out5            ! 7 finished local3 used

	and	local0, 252, local0
	ld	[local6+local5], local5
	xor	out5, local2, out5            ! 2 finished local2 now sbox 3

	srl	local7, 8, local2         ! 3 start
	ld	[out3+local0], local0
	xor	out5, local4, out5

	and	local2, 252, local2
	ld	[global1+local1], local1
	xor	out5, local5, out5            ! 6 finished local5 used

	ld	[global3+local2], local2
	srl	in5, 3, local3
	xor	out5, local0, out5

	ld	[in4+4], out1 ! key next encryption/decryption
	sll	in5, 29, local4
	xor	out5, local1, out5

	retl
	xor	out5, local2, out5



	.align 32

.des_dec:

	! implemented with out5 as first parameter to avoid
	! register exchange in ede modes

	! key address in4
	! loads key next encryption/decryption first round from [in3]

	

! rounds_macro
! out5 in5 -1 .des_dec.1 in4 in3 retl  

	xor	in5, out0, local1

	ld	[out2+284], local5        ! 0x0000FC00
	ba	.des_dec.1
	and	local1, 252, local1

	.align 32

.des_dec.1:
	! local6 is address sbox 6
	! out3   is address sbox 8
	! out4   is loop counter

	ld	[global1+local1], local1
	xor	in5, out1, out1            ! 8642
	xor	in5, out0, out0            ! 7531
	! fmovs	%f0, %f0                  ! fxor used for alignment

	srl	out1, 4, local0           ! rotate 4 right
	and	out0, local5, local3      ! 3
	! fmovs	%f0, %f0

	ld	[in4+-1*8], local7         ! key 7531 next round
	srl	local3, 8, local3         ! 3
	and	local0, 252, local2       ! 2
	! fmovs	%f0, %f0

	ld	[global3+local3],local3   ! 3
	sll	out1, 28, out1            ! rotate
	xor	out5, local1, out5            ! 1 finished, local1 now sbox 7

	ld	[global2+local2], local2  ! 2 
	srl	out0, 24, local1          ! 7
	or	out1, local0, out1        ! rotate

	ldub	[out2+local1], local1     ! 7 (and 0xFC)
	srl	out1, 24, local0          ! 8
	and	out1, local5, local4      ! 4

	ldub	[out2+local0], local0     ! 8 (and 0xFC)
	srl	local4, 8, local4         ! 4
	xor	out5, local2, out5            ! 2 finished local2 now sbox 6

	ld	[global4+local4],local4   ! 4
	srl	out1, 16, local2          ! 6
	xor	out5, local3, out5            ! 3 finished local3 now sbox 5

	ld	[out3+local0],local0      ! 8
	and	local2, 252, local2       ! 6
	add	global1, 1536, local5     ! address sbox 7

	ld	[local6+local2], local2   ! 6
	srl	out0, 16, local3          ! 5
	xor	out5, local4, out5            ! 4 finished

	ld	[local5+local1],local1    ! 7
	and	local3, 252, local3       ! 5
	xor	out5, local0, out5            ! 8 finished

	ld	[global5+local3],local3   ! 5
	xor	out5, local2, out5            ! 6 finished
	subcc	out4, 1, out4

	ld	[in4+-1*8+4], out0         ! key 8642 next round
	xor	out5, local7, local2        ! sbox 5 next round
	xor	out5, local1, out5            ! 7 finished

	srl	local2, 16, local2        ! sbox 5 next round
	xor	out5, local3, out5            ! 5 finished

	ld	[in4+-1*16+4], out1        ! key 8642 next round again
	and	local2, 252, local2       ! sbox5 next round
! next round
	xor	out5, local7, local7        ! 7531

	ld	[global5+local2], local2  ! 5
	srl	local7, 24, local3        ! 7
	xor	out5, out0, out0            ! 8642

	ldub	[out2+local3], local3     ! 7 (and 0xFC)
	srl	out0, 4, local0           ! rotate 4 right
	and	local7, 252, local1       ! 1

	sll	out0, 28, out0            ! rotate
	xor	in5, local2, in5            ! 5 finished local2 used

	srl	local0, 8, local4         ! 4
	and	local0, 252, local2       ! 2
	ld	[local5+local3], local3   ! 7

	srl	local0, 16, local5        ! 6
	or	out0, local0, out0        ! rotate
	ld	[global2+local2], local2  ! 2

	srl	out0, 24, local0
	ld	[in4+-1*16], out0          ! key 7531 next round
	and	local4, 252, local4	  ! 4

	and	local5, 252, local5       ! 6
	ld	[global4+local4], local4  ! 4
	xor	in5, local3, in5            ! 7 finished local3 used

	and	local0, 252, local0       ! 8
	ld	[local6+local5], local5   ! 6
	xor	in5, local2, in5            ! 2 finished local2 now sbox 3

	srl	local7, 8, local2         ! 3 start
	ld	[out3+local0], local0     ! 8
	xor	in5, local4, in5            ! 4 finished

	and	local2, 252, local2       ! 3
	ld	[global1+local1], local1  ! 1
	xor	in5, local5, in5            ! 6 finished local5 used

	ld	[global3+local2], local2  ! 3
	xor	in5, local0, in5            ! 8 finished
	add	in4, -1*16, in4             ! enc add 8, dec add -8 to key pointer

	ld	[out2+284], local5        ! 0x0000FC00
	xor	in5, out0, local4          ! sbox 1 next round
	xor	in5, local1, in5            ! 1 finished

	xor	in5, local2, in5            ! 3 finished
#ifdef __sparc_v9__
	bne,pt	%icc, .des_dec.1
#else
	bne	.des_dec.1
#endif
	and	local4, 252, local1       ! sbox 1 next round

! two rounds more:

	ld	[global1+local1], local1
	xor	in5, out1, out1
	xor	in5, out0, out0

	srl	out1, 4, local0           ! rotate
	and	out0, local5, local3

	ld	[in4+-1*8], local7         ! key 7531
	srl	local3, 8, local3
	and	local0, 252, local2

	ld	[global3+local3],local3
	sll	out1, 28, out1            ! rotate
	xor	out5, local1, out5            ! 1 finished, local1 now sbox 7

	ld	[global2+local2], local2
	srl	out0, 24, local1
	or	out1, local0, out1        ! rotate

	ldub	[out2+local1], local1
	srl	out1, 24, local0
	and	out1, local5, local4

	ldub	[out2+local0], local0
	srl	local4, 8, local4
	xor	out5, local2, out5            ! 2 finished local2 now sbox 6

	ld	[global4+local4],local4
	srl	out1, 16, local2
	xor	out5, local3, out5            ! 3 finished local3 now sbox 5

	ld	[out3+local0],local0
	and	local2, 252, local2
	add	global1, 1536, local5     ! address sbox 7

	ld	[local6+local2], local2
	srl	out0, 16, local3
	xor	out5, local4, out5            ! 4 finished

	ld	[local5+local1],local1
	and	local3, 252, local3
	xor	out5, local0, out5

	ld	[global5+local3],local3
	xor	out5, local2, out5            ! 6 finished
	cmp	in2, 8

	ld	[out2+280], out4  ! loop counter
	xor	out5, local7, local2        ! sbox 5 next round
	xor	out5, local1, out5            ! 7 finished

	ld	[in4+-1*8+4], out0
	srl	local2, 16, local2        ! sbox 5 next round
	xor	out5, local3, out5            ! 5 finished

	and	local2, 252, local2
! next round (two rounds more)
	xor	out5, local7, local7        ! 7531

	ld	[global5+local2], local2
	srl	local7, 24, local3
	xor	out5, out0, out0            ! 8642

	ldub	[out2+local3], local3
	srl	out0, 4, local0           ! rotate
	and	local7, 252, local1

	sll	out0, 28, out0            ! rotate
	xor	in5, local2, in5            ! 5 finished local2 used

	srl	local0, 8, local4
	and	local0, 252, local2
	ld	[local5+local3], local3

	srl	local0, 16, local5
	or	out0, local0, out0        ! rotate
	ld	[global2+local2], local2

	srl	out0, 24, local0
	ld	[in3], out0   ! key next encryption/decryption
	and	local4, 252, local4

	and	local5, 252, local5
	ld	[global4+local4], local4
	xor	in5, local3, in5            ! 7 finished local3 used

	and	local0, 252, local0
	ld	[local6+local5], local5
	xor	in5, local2, in5            ! 2 finished local2 now sbox 3

	srl	local7, 8, local2         ! 3 start
	ld	[out3+local0], local0
	xor	in5, local4, in5

	and	local2, 252, local2
	ld	[global1+local1], local1
	xor	in5, local5, in5            ! 6 finished local5 used

	ld	[global3+local2], local2
	srl	out5, 3, local3
	xor	in5, local0, in5

	ld	[in3+4], out1 ! key next encryption/decryption
	sll	out5, 29, local4
	xor	in5, local1, in5

	retl
	xor	in5, local2, in5




! void DES_encrypt1(data, ks, enc)
! *******************************

	.align 32
	.global DES_encrypt1
	.type	 DES_encrypt1,#function

DES_encrypt1:

	save	%sp, FRAME, %sp

	sethi	%hi(_PIC_DES_SPtrans-1f),global1
	or	global1,%lo(_PIC_DES_SPtrans-1f),global1
1:	call	.+8
	add	%o7,global1,global1
	sub	global1,_PIC_DES_SPtrans-.des_and,out2

	ld	[in0], in5                ! left
	cmp	in2, 0                    ! enc

#ifdef __sparc_v9__
	be,pn	%icc, .encrypt.dec        ! enc/dec
#else
	be	.encrypt.dec
#endif
	ld	[in0+4], out5             ! right

	! parameter 6  1/2 for include encryption/decryption
	! parameter 7  1 for move in1 to in3
	! parameter 8  1 for move in3 to in4, 2 for move in4 to in3

	

! ip_macro
! in5 out5 out5 in5 in3 0 1 1 

	ld	[out2+256], local1
	srl	out5, 4, local4

	xor	local4, in5, local4
	mov in1, in3

	ld	[out2+260], local2
	and	local4, local1, local4
	mov in3, in4
	

	ld	[out2+280], out4          ! loop counter
	sll	local4, 4, local1
	xor	in5, local4, in5

	ld	[out2+264], local3
	srl	in5, 16, local4
	xor	out5, local1, out5

	
	xor	local4, out5, local4
	nop	!sethi	%hi(DES_SPtrans), global1 ! sbox addr

	
	and	local4, local2, local4
	nop	!or	global1, %lo(DES_SPtrans), global1   ! sbox addr

	sll	local4, 16, local1
	xor	out5, local4, out5

	srl	out5, 2, local4
	xor	in5, local1, in5

	sethi	%hi(16711680), local5
	xor	local4, in5, local4

	and	local4, local3, local4
	or	local5, 255, local5

	sll	local4, 2, local2
	xor	in5, local4, in5

	srl	in5, 8, local4
	xor	out5, local2, out5

	xor	local4, out5, local4
	add	global1, 768, global4

	and	local4, local5, local4
	add	global1, 1024, global5

	ld	[out2+272], local7
	sll	local4, 8, local1
	xor	out5, local4, out5

	srl	out5, 1, local4
	xor	in5, local1, in5

	ld	[in3], out0                ! key 7531
	xor	local4, in5, local4
	add	global1, 256, global2

	ld	[in3+4], out1              ! key 8642
	and	local4, local7, local4
	add	global1, 512, global3

	sll	local4, 1, local1
	xor	in5, local4, in5

	sll	in5, 3, local3
	xor	out5, local1, out5

	sll	out5, 3, local2
	add	global1, 1280, local6     ! address sbox 8

	srl	in5, 29, local4
	add	global1, 1792, out3       ! address sbox 8

	srl	out5, 29, local1
	or	local4, local3, out5

	or	local2, local1, in5

	

	


	

! rounds_macro
! in5 out5 1 .des_encrypt1.1 in3 in4   

	xor	out5, out0, local1

	ld	[out2+284], local5        ! 0x0000FC00
	ba	.des_encrypt1.1
	and	local1, 252, local1

	.align 32

.des_encrypt1.1:
	! local6 is address sbox 6
	! out3   is address sbox 8
	! out4   is loop counter

	ld	[global1+local1], local1
	xor	out5, out1, out1            ! 8642
	xor	out5, out0, out0            ! 7531
	! fmovs	%f0, %f0                  ! fxor used for alignment

	srl	out1, 4, local0           ! rotate 4 right
	and	out0, local5, local3      ! 3
	! fmovs	%f0, %f0

	ld	[in3+1*8], local7         ! key 7531 next round
	srl	local3, 8, local3         ! 3
	and	local0, 252, local2       ! 2
	! fmovs	%f0, %f0

	ld	[global3+local3],local3   ! 3
	sll	out1, 28, out1            ! rotate
	xor	in5, local1, in5            ! 1 finished, local1 now sbox 7

	ld	[global2+local2], local2  ! 2 
	srl	out0, 24, local1          ! 7
	or	out1, local0, out1        ! rotate

	ldub	[out2+local1], local1     ! 7 (and 0xFC)
	srl	out1, 24, local0          ! 8
	and	out1, local5, local4      ! 4

	ldub	[out2+local0], local0     ! 8 (and 0xFC)
	srl	local4, 8, local4         ! 4
	xor	in5, local2, in5            ! 2 finished local2 now sbox 6

	ld	[global4+local4],local4   ! 4
	srl	out1, 16, local2          ! 6
	xor	in5, local3, in5            ! 3 finished local3 now sbox 5

	ld	[out3+local0],local0      ! 8
	and	local2, 252, local2       ! 6
	add	global1, 1536, local5     ! address sbox 7

	ld	[local6+local2], local2   ! 6
	srl	out0, 16, local3          ! 5
	xor	in5, local4, in5            ! 4 finished

	ld	[local5+local1],local1    ! 7
	and	local3, 252, local3       ! 5
	xor	in5, local0, in5            ! 8 finished

	ld	[global5+local3],local3   ! 5
	xor	in5, local2, in5            ! 6 finished
	subcc	out4, 1, out4

	ld	[in3+1*8+4], out0         ! key 8642 next round
	xor	in5, local7, local2        ! sbox 5 next round
	xor	in5, local1, in5            ! 7 finished

	srl	local2, 16, local2        ! sbox 5 next round
	xor	in5, local3, in5            ! 5 finished

	ld	[in3+1*16+4], out1        ! key 8642 next round again
	and	local2, 252, local2       ! sbox5 next round
! next round
	xor	in5, local7, local7        ! 7531

	ld	[global5+local2], local2  ! 5
	srl	local7, 24, local3        ! 7
	xor	in5, out0, out0            ! 8642

	ldub	[out2+local3], local3     ! 7 (and 0xFC)
	srl	out0, 4, local0           ! rotate 4 right
	and	local7, 252, local1       ! 1

	sll	out0, 28, out0            ! rotate
	xor	out5, local2, out5            ! 5 finished local2 used

	srl	local0, 8, local4         ! 4
	and	local0, 252, local2       ! 2
	ld	[local5+local3], local3   ! 7

	srl	local0, 16, local5        ! 6
	or	out0, local0, out0        ! rotate
	ld	[global2+local2], local2  ! 2

	srl	out0, 24, local0
	ld	[in3+1*16], out0          ! key 7531 next round
	and	local4, 252, local4	  ! 4

	and	local5, 252, local5       ! 6
	ld	[global4+local4], local4  ! 4
	xor	out5, local3, out5            ! 7 finished local3 used

	and	local0, 252, local0       ! 8
	ld	[local6+local5], local5   ! 6
	xor	out5, local2, out5            ! 2 finished local2 now sbox 3

	srl	local7, 8, local2         ! 3 start
	ld	[out3+local0], local0     ! 8
	xor	out5, local4, out5            ! 4 finished

	and	local2, 252, local2       ! 3
	ld	[global1+local1], local1  ! 1
	xor	out5, local5, out5            ! 6 finished local5 used

	ld	[global3+local2], local2  ! 3
	xor	out5, local0, out5            ! 8 finished
	add	in3, 1*16, in3             ! enc add 8, dec add -8 to key pointer

	ld	[out2+284], local5        ! 0x0000FC00
	xor	out5, out0, local4          ! sbox 1 next round
	xor	out5, local1, out5            ! 1 finished

	xor	out5, local2, out5            ! 3 finished
#ifdef __sparc_v9__
	bne,pt	%icc, .des_encrypt1.1
#else
	bne	.des_encrypt1.1
#endif
	and	local4, 252, local1       ! sbox 1 next round

! two rounds more:

	ld	[global1+local1], local1
	xor	out5, out1, out1
	xor	out5, out0, out0

	srl	out1, 4, local0           ! rotate
	and	out0, local5, local3

	ld	[in3+1*8], local7         ! key 7531
	srl	local3, 8, local3
	and	local0, 252, local2

	ld	[global3+local3],local3
	sll	out1, 28, out1            ! rotate
	xor	in5, local1, in5            ! 1 finished, local1 now sbox 7

	ld	[global2+local2], local2
	srl	out0, 24, local1
	or	out1, local0, out1        ! rotate

	ldub	[out2+local1], local1
	srl	out1, 24, local0
	and	out1, local5, local4

	ldub	[out2+local0], local0
	srl	local4, 8, local4
	xor	in5, local2, in5            ! 2 finished local2 now sbox 6

	ld	[global4+local4],local4
	srl	out1, 16, local2
	xor	in5, local3, in5            ! 3 finished local3 now sbox 5

	ld	[out3+local0],local0
	and	local2, 252, local2
	add	global1, 1536, local5     ! address sbox 7

	ld	[local6+local2], local2
	srl	out0, 16, local3
	xor	in5, local4, in5            ! 4 finished

	ld	[local5+local1],local1
	and	local3, 252, local3
	xor	in5, local0, in5

	ld	[global5+local3],local3
	xor	in5, local2, in5            ! 6 finished
	cmp	in2, 8

	ld	[out2+280], out4  ! loop counter
	xor	in5, local7, local2        ! sbox 5 next round
	xor	in5, local1, in5            ! 7 finished

	ld	[in3+1*8+4], out0
	srl	local2, 16, local2        ! sbox 5 next round
	xor	in5, local3, in5            ! 5 finished

	and	local2, 252, local2
! next round (two rounds more)
	xor	in5, local7, local7        ! 7531

	ld	[global5+local2], local2
	srl	local7, 24, local3
	xor	in5, out0, out0            ! 8642

	ldub	[out2+local3], local3
	srl	out0, 4, local0           ! rotate
	and	local7, 252, local1

	sll	out0, 28, out0            ! rotate
	xor	out5, local2, out5            ! 5 finished local2 used

	srl	local0, 8, local4
	and	local0, 252, local2
	ld	[local5+local3], local3

	srl	local0, 16, local5
	or	out0, local0, out0        ! rotate
	ld	[global2+local2], local2

	srl	out0, 24, local0
	ld	[in4], out0   ! key next encryption/decryption
	and	local4, 252, local4

	and	local5, 252, local5
	ld	[global4+local4], local4
	xor	out5, local3, out5            ! 7 finished local3 used

	and	local0, 252, local0
	ld	[local6+local5], local5
	xor	out5, local2, out5            ! 2 finished local2 now sbox 3

	srl	local7, 8, local2         ! 3 start
	ld	[out3+local0], local0
	xor	out5, local4, out5

	and	local2, 252, local2
	ld	[global1+local1], local1
	xor	out5, local5, out5            ! 6 finished local5 used

	ld	[global3+local2], local2
	srl	in5, 3, local3
	xor	out5, local0, out5

	ld	[in4+4], out1 ! key next encryption/decryption
	sll	in5, 29, local4
	xor	out5, local1, out5

	
	xor	out5, local2, out5
 ! in4 not used

	

! fp_macro
! in5 out5 1      

	! initially undo the rotate 3 left done after initial permutation
	! original left is received shifted 3 right and 29 left in local3/4

	sll	out5, 29, local1
	or	local3, local4, in5

	srl	out5, 3, out5
	sethi	%hi(0x55555555), local2

	or	out5, local1, out5
	or	local2, %lo(0x55555555), local2

	srl	out5, 1, local3
	sethi	%hi(0x00ff00ff), local1
	xor	local3, in5, local3
	or	local1, %lo(0x00ff00ff), local1
	and	local3, local2, local3
	sethi	%hi(0x33333333), local4
	sll	local3, 1, local2

	xor	in5, local3, in5

	srl	in5, 8, local3
	xor	out5, local2, out5
	xor	local3, out5, local3
	or	local4, %lo(0x33333333), local4
	and	local3, local1, local3
	sethi	%hi(0x0000ffff), local1
	sll	local3, 8, local2

	xor	out5, local3, out5

	srl	out5, 2, local3
	xor	in5, local2, in5
	xor	local3, in5, local3
	or	local1, %lo(0x0000ffff), local1
	and	local3, local4, local3
	sethi	%hi(0x0f0f0f0f), local4
	sll	local3, 2, local2

	
	xor	in5, local3, in5

	
	srl	in5, 16, local3
	xor	out5, local2, out5
	xor	local3, out5, local3
	or	local4, %lo(0x0f0f0f0f), local4
	and	local3, local1, local3
	sll	local3, 16, local2

	xor	out5, local3, local1

	srl	local1, 4, local3
	xor	in5, local2, in5
	xor	local3, in5, local3
	and	local3, local4, local3
	sll	local3, 4, local2

	xor	in5, local3, in5

	! optional store:

	st in5, [in0]

	xor	local1, local2, out5

	st out5, [in0+4]

            ! 1 for store to [in0]

	ret
	restore

.encrypt.dec:

	add	in1, 120, in3             ! use last subkey for first round

	! parameter 6  1/2 for include encryption/decryption
	! parameter 7  1 for move in1 to in3
	! parameter 8  1 for move in3 to in4, 2 for move in4 to in3

	

! ip_macro
! in5 out5 in5 out5 in4 2 0 1 

	ld	[out2+256], local1
	srl	out5, 4, local4

	xor	local4, in5, local4
	nop

	ld	[out2+260], local2
	and	local4, local1, local4
	mov in3, in4
	

	ld	[out2+280], out4          ! loop counter
	sll	local4, 4, local1
	xor	in5, local4, in5

	ld	[out2+264], local3
	srl	in5, 16, local4
	xor	out5, local1, out5

	
	xor	local4, out5, local4
	nop	!sethi	%hi(DES_SPtrans), global1 ! sbox addr

	
	and	local4, local2, local4
	nop	!or	global1, %lo(DES_SPtrans), global1   ! sbox addr

	sll	local4, 16, local1
	xor	out5, local4, out5

	srl	out5, 2, local4
	xor	in5, local1, in5

	sethi	%hi(16711680), local5
	xor	local4, in5, local4

	and	local4, local3, local4
	or	local5, 255, local5

	sll	local4, 2, local2
	xor	in5, local4, in5

	srl	in5, 8, local4
	xor	out5, local2, out5

	xor	local4, out5, local4
	add	global1, 768, global4

	and	local4, local5, local4
	add	global1, 1024, global5

	ld	[out2+272], local7
	sll	local4, 8, local1
	xor	out5, local4, out5

	srl	out5, 1, local4
	xor	in5, local1, in5

	ld	[in4], out0                ! key 7531
	xor	local4, in5, local4
	add	global1, 256, global2

	ld	[in4+4], out1              ! key 8642
	and	local4, local7, local4
	add	global1, 512, global3

	sll	local4, 1, local1
	xor	in5, local4, in5

	sll	in5, 3, local3
	xor	out5, local1, out5

	sll	out5, 3, local2
	add	global1, 1280, local6     ! address sbox 8

	srl	in5, 29, local4
	add	global1, 1792, out3       ! address sbox 8

	srl	out5, 29, local1
	or	local4, local3, in5

	or	local2, local1, out5

	

	

		ld	[out2+284], local5     ! 0x0000FC00 used in the rounds
		or	local2, local1, out5
		xor	in5, out0, local1

		call .des_dec.1
		and	local1, 252, local1

	
 ! include dec,  ks in4

	

! fp_macro
! out5 in5 1      

	! initially undo the rotate 3 left done after initial permutation
	! original left is received shifted 3 right and 29 left in local3/4

	sll	in5, 29, local1
	or	local3, local4, out5

	srl	in5, 3, in5
	sethi	%hi(0x55555555), local2

	or	in5, local1, in5
	or	local2, %lo(0x55555555), local2

	srl	in5, 1, local3
	sethi	%hi(0x00ff00ff), local1
	xor	local3, out5, local3
	or	local1, %lo(0x00ff00ff), local1
	and	local3, local2, local3
	sethi	%hi(0x33333333), local4
	sll	local3, 1, local2

	xor	out5, local3, out5

	srl	out5, 8, local3
	xor	in5, local2, in5
	xor	local3, in5, local3
	or	local4, %lo(0x33333333), local4
	and	local3, local1, local3
	sethi	%hi(0x0000ffff), local1
	sll	local3, 8, local2

	xor	in5, local3, in5

	srl	in5, 2, local3
	xor	out5, local2, out5
	xor	local3, out5, local3
	or	local1, %lo(0x0000ffff), local1
	and	local3, local4, local3
	sethi	%hi(0x0f0f0f0f), local4
	sll	local3, 2, local2

	
	xor	out5, local3, out5

	
	srl	out5, 16, local3
	xor	in5, local2, in5
	xor	local3, in5, local3
	or	local4, %lo(0x0f0f0f0f), local4
	and	local3, local1, local3
	sll	local3, 16, local2

	xor	in5, local3, local1

	srl	local1, 4, local3
	xor	out5, local2, out5
	xor	local3, out5, local3
	and	local3, local4, local3
	sll	local3, 4, local2

	xor	out5, local3, out5

	! optional store:

	st out5, [in0]

	xor	local1, local2, in5

	st in5, [in0+4]

            ! 1 for store to [in0]

	ret
	restore

.DES_encrypt1.end:
	.size	 DES_encrypt1,.DES_encrypt1.end-DES_encrypt1


! void DES_encrypt2(data, ks, enc)
!*********************************

	! encrypts/decrypts without initial/final permutation

	.align 32
	.global DES_encrypt2
	.type	 DES_encrypt2,#function

DES_encrypt2:

	save	%sp, FRAME, %sp

	sethi	%hi(_PIC_DES_SPtrans-1f),global1
	or	global1,%lo(_PIC_DES_SPtrans-1f),global1
1:	call	.+8
	add	%o7,global1,global1
	sub	global1,_PIC_DES_SPtrans-.des_and,out2

	! Set sbox address 1 to 6 and rotate halfs 3 left
	! Errors caught by destest? Yes. Still? *NO*

	!sethi	%hi(DES_SPtrans), global1 ! address sbox 1

	!or	global1, %lo(DES_SPtrans), global1  ! sbox 1

	add	global1, 256, global2     ! sbox 2
	add	global1, 512, global3     ! sbox 3

	ld	[in0], out5               ! right
	add	global1, 768, global4     ! sbox 4
	add	global1, 1024, global5    ! sbox 5

	ld	[in0+4], in5              ! left
	add	global1, 1280, local6     ! sbox 6
	add	global1, 1792, out3       ! sbox 8

	! rotate

	sll	in5, 3, local5
	mov	in1, in3                  ! key address to in3

	sll	out5, 3, local7
	srl	in5, 29, in5

	srl	out5, 29, out5
	add	in5, local5, in5

	add	out5, local7, out5
	cmp	in2, 0

	! we use our own stackframe

#ifdef __sparc_v9__
	be,pn	%icc, .encrypt2.dec       ! decryption
#else
	be	.encrypt2.dec
#endif
	STPTR	in0, [%sp+BIAS+ARG0+0*ARGSZ]

	ld	[in3], out0               ! key 7531 first round
	mov	LOOPS, out4               ! loop counter

	ld	[in3+4], out1             ! key 8642 first round
	sethi	%hi(0x0000FC00), local5

	call .des_enc
	mov	in3, in4

	! rotate
	sll	in5, 29, in0
	srl	in5, 3, in5
	sll	out5, 29, in1
	add	in5, in0, in5
	srl	out5, 3, out5
	LDPTR	[%sp+BIAS+ARG0+0*ARGSZ], in0
	add	out5, in1, out5
	st	in5, [in0]
	st	out5, [in0+4]

	ret
	restore


.encrypt2.dec:

	add in3, 120, in4

	ld	[in4], out0               ! key 7531 first round
	mov	LOOPS, out4               ! loop counter

	ld	[in4+4], out1             ! key 8642 first round
	sethi	%hi(0x0000FC00), local5

	mov	in5, local1               ! left expected in out5
	mov	out5, in5

	call .des_dec
	mov	local1, out5

.encrypt2.finish:

	! rotate
	sll	in5, 29, in0
	srl	in5, 3, in5
	sll	out5, 29, in1
	add	in5, in0, in5
	srl	out5, 3, out5
	LDPTR	[%sp+BIAS+ARG0+0*ARGSZ], in0
	add	out5, in1, out5
	st	out5, [in0]
	st	in5, [in0+4]

	ret
	restore

.DES_encrypt2.end:
	.size	 DES_encrypt2, .DES_encrypt2.end-DES_encrypt2


! void DES_encrypt3(data, ks1, ks2, ks3)
! **************************************

	.align 32
	.global DES_encrypt3
	.type	 DES_encrypt3,#function

DES_encrypt3:

	save	%sp, FRAME, %sp
	
	sethi	%hi(_PIC_DES_SPtrans-1f),global1
	or	global1,%lo(_PIC_DES_SPtrans-1f),global1
1:	call	.+8
	add	%o7,global1,global1
	sub	global1,_PIC_DES_SPtrans-.des_and,out2

	ld	[in0], in5                ! left
	add	in2, 120, in4             ! ks2

	ld	[in0+4], out5             ! right
	mov	in3, in2                  ! save ks3

	! parameter 6  1/2 for include encryption/decryption
	! parameter 7  1 for mov in1 to in3
	! parameter 8  1 for mov in3 to in4
	! parameter 9  1 for load ks3 and ks2 to in4 and in3

	

! ip_macro
! in5 out5 out5 in5 in3 1 1 0 0

	ld	[out2+256], local1
	srl	out5, 4, local4

	xor	local4, in5, local4
	mov in1, in3

	ld	[out2+260], local2
	and	local4, local1, local4
	
	

	ld	[out2+280], out4          ! loop counter
	sll	local4, 4, local1
	xor	in5, local4, in5

	ld	[out2+264], local3
	srl	in5, 16, local4
	xor	out5, local1, out5

	
	xor	local4, out5, local4
	nop	!sethi	%hi(DES_SPtrans), global1 ! sbox addr

	
	and	local4, local2, local4
	nop	!or	global1, %lo(DES_SPtrans), global1   ! sbox addr

	sll	local4, 16, local1
	xor	out5, local4, out5

	srl	out5, 2, local4
	xor	in5, local1, in5

	sethi	%hi(16711680), local5
	xor	local4, in5, local4

	and	local4, local3, local4
	or	local5, 255, local5

	sll	local4, 2, local2
	xor	in5, local4, in5

	srl	in5, 8, local4
	xor	out5, local2, out5

	xor	local4, out5, local4
	add	global1, 768, global4

	and	local4, local5, local4
	add	global1, 1024, global5

	ld	[out2+272], local7
	sll	local4, 8, local1
	xor	out5, local4, out5

	srl	out5, 1, local4
	xor	in5, local1, in5

	ld	[in3], out0                ! key 7531
	xor	local4, in5, local4
	add	global1, 256, global2

	ld	[in3+4], out1              ! key 8642
	and	local4, local7, local4
	add	global1, 512, global3

	sll	local4, 1, local1
	xor	in5, local4, in5

	sll	in5, 3, local3
	xor	out5, local1, out5

	sll	out5, 3, local2
	add	global1, 1280, local6     ! address sbox 8

	srl	in5, 29, local4
	add	global1, 1792, out3       ! address sbox 8

	srl	out5, 29, local1
	or	local4, local3, out5

	or	local2, local1, in5

	

		ld	[out2+284], local5     ! 0x0000FC00 used in the rounds
		or	local2, local1, in5
		xor	out5, out0, local1

		call .des_enc.1
		and	local1, 252, local1

	

	


	call	.des_dec
	mov	in2, in3                  ! preload ks3

	call	.des_enc
	nop

	

! fp_macro
! in5 out5 1      

	! initially undo the rotate 3 left done after initial permutation
	! original left is received shifted 3 right and 29 left in local3/4

	sll	out5, 29, local1
	or	local3, local4, in5

	srl	out5, 3, out5
	sethi	%hi(0x55555555), local2

	or	out5, local1, out5
	or	local2, %lo(0x55555555), local2

	srl	out5, 1, local3
	sethi	%hi(0x00ff00ff), local1
	xor	local3, in5, local3
	or	local1, %lo(0x00ff00ff), local1
	and	local3, local2, local3
	sethi	%hi(0x33333333), local4
	sll	local3, 1, local2

	xor	in5, local3, in5

	srl	in5, 8, local3
	xor	out5, local2, out5
	xor	local3, out5, local3
	or	local4, %lo(0x33333333), local4
	and	local3, local1, local3
	sethi	%hi(0x0000ffff), local1
	sll	local3, 8, local2

	xor	out5, local3, out5

	srl	out5, 2, local3
	xor	in5, local2, in5
	xor	local3, in5, local3
	or	local1, %lo(0x0000ffff), local1
	and	local3, local4, local3
	sethi	%hi(0x0f0f0f0f), local4
	sll	local3, 2, local2

	
	xor	in5, local3, in5

	
	srl	in5, 16, local3
	xor	out5, local2, out5
	xor	local3, out5, local3
	or	local4, %lo(0x0f0f0f0f), local4
	and	local3, local1, local3
	sll	local3, 16, local2

	xor	out5, local3, local1

	srl	local1, 4, local3
	xor	in5, local2, in5
	xor	local3, in5, local3
	and	local3, local4, local3
	sll	local3, 4, local2

	xor	in5, local3, in5

	! optional store:

	st in5, [in0]

	xor	local1, local2, out5

	st out5, [in0+4]



	ret
	restore

.DES_encrypt3.end:
	.size	 DES_encrypt3,.DES_encrypt3.end-DES_encrypt3


! void DES_decrypt3(data, ks1, ks2, ks3)
! **************************************

	.align 32
	.global DES_decrypt3
	.type	 DES_decrypt3,#function

DES_decrypt3:

	save	%sp, FRAME, %sp
	
	sethi	%hi(_PIC_DES_SPtrans-1f),global1
	or	global1,%lo(_PIC_DES_SPtrans-1f),global1
1:	call	.+8
	add	%o7,global1,global1
	sub	global1,_PIC_DES_SPtrans-.des_and,out2

	ld	[in0], in5                ! left
	add	in3, 120, in4             ! ks3

	ld	[in0+4], out5             ! right
	mov	in2, in3                  ! ks2

	! parameter 6  1/2 for include encryption/decryption
	! parameter 7  1 for mov in1 to in3
	! parameter 8  1 for mov in3 to in4
	! parameter 9  1 for load ks3 and ks2 to in4 and in3

	

! ip_macro
! in5 out5 in5 out5 in4 2 0 0 0

	ld	[out2+256], local1
	srl	out5, 4, local4

	xor	local4, in5, local4
	nop

	ld	[out2+260], local2
	and	local4, local1, local4
	
	

	ld	[out2+280], out4          ! loop counter
	sll	local4, 4, local1
	xor	in5, local4, in5

	ld	[out2+264], local3
	srl	in5, 16, local4
	xor	out5, local1, out5

	
	xor	local4, out5, local4
	nop	!sethi	%hi(DES_SPtrans), global1 ! sbox addr

	
	and	local4, local2, local4
	nop	!or	global1, %lo(DES_SPtrans), global1   ! sbox addr

	sll	local4, 16, local1
	xor	out5, local4, out5

	srl	out5, 2, local4
	xor	in5, local1, in5

	sethi	%hi(16711680), local5
	xor	local4, in5, local4

	and	local4, local3, local4
	or	local5, 255, local5

	sll	local4, 2, local2
	xor	in5, local4, in5

	srl	in5, 8, local4
	xor	out5, local2, out5

	xor	local4, out5, local4
	add	global1, 768, global4

	and	local4, local5, local4
	add	global1, 1024, global5

	ld	[out2+272], local7
	sll	local4, 8, local1
	xor	out5, local4, out5

	srl	out5, 1, local4
	xor	in5, local1, in5

	ld	[in4], out0                ! key 7531
	xor	local4, in5, local4
	add	global1, 256, global2

	ld	[in4+4], out1              ! key 8642
	and	local4, local7, local4
	add	global1, 512, global3

	sll	local4, 1, local1
	xor	in5, local4, in5

	sll	in5, 3, local3
	xor	out5, local1, out5

	sll	out5, 3, local2
	add	global1, 1280, local6     ! address sbox 8

	srl	in5, 29, local4
	add	global1, 1792, out3       ! address sbox 8

	srl	out5, 29, local1
	or	local4, local3, in5

	or	local2, local1, out5

	

	

		ld	[out2+284], local5     ! 0x0000FC00 used in the rounds
		or	local2, local1, out5
		xor	in5, out0, local1

		call .des_dec.1
		and	local1, 252, local1

	


	call	.des_enc
	add	in1, 120, in4             ! preload ks1

	call	.des_dec
	nop

	

! fp_macro
! out5 in5 1      

	! initially undo the rotate 3 left done after initial permutation
	! original left is received shifted 3 right and 29 left in local3/4

	sll	in5, 29, local1
	or	local3, local4, out5

	srl	in5, 3, in5
	sethi	%hi(0x55555555), local2

	or	in5, local1, in5
	or	local2, %lo(0x55555555), local2

	srl	in5, 1, local3
	sethi	%hi(0x00ff00ff), local1
	xor	local3, out5, local3
	or	local1, %lo(0x00ff00ff), local1
	and	local3, local2, local3
	sethi	%hi(0x33333333), local4
	sll	local3, 1, local2

	xor	out5, local3, out5

	srl	out5, 8, local3
	xor	in5, local2, in5
	xor	local3, in5, local3
	or	local4, %lo(0x33333333), local4
	and	local3, local1, local3
	sethi	%hi(0x0000ffff), local1
	sll	local3, 8, local2

	xor	in5, local3, in5

	srl	in5, 2, local3
	xor	out5, local2, out5
	xor	local3, out5, local3
	or	local1, %lo(0x0000ffff), local1
	and	local3, local4, local3
	sethi	%hi(0x0f0f0f0f), local4
	sll	local3, 2, local2

	
	xor	out5, local3, out5

	
	srl	out5, 16, local3
	xor	in5, local2, in5
	xor	local3, in5, local3
	or	local4, %lo(0x0f0f0f0f), local4
	and	local3, local1, local3
	sll	local3, 16, local2

	xor	in5, local3, local1

	srl	local1, 4, local3
	xor	out5, local2, out5
	xor	local3, out5, local3
	and	local3, local4, local3
	sll	local3, 4, local2

	xor	out5, local3, out5

	! optional store:

	st out5, [in0]

	xor	local1, local2, in5

	st in5, [in0+4]



	ret
	restore

.DES_decrypt3.end:
	.size	 DES_decrypt3,.DES_decrypt3.end-DES_decrypt3

! void DES_ncbc_encrypt(input, output, length, schedule, ivec, enc)
! *****************************************************************


	.align 32
	.global DES_ncbc_encrypt
	.type	 DES_ncbc_encrypt,#function

DES_ncbc_encrypt:

	save	%sp, FRAME, %sp
	
	
	
	

	sethi	%hi(_PIC_DES_SPtrans-1f),global1
	or	global1,%lo(_PIC_DES_SPtrans-1f),global1
1:	call	.+8
	add	%o7,global1,global1
	sub	global1,_PIC_DES_SPtrans-.des_and,out2

	cmp	in5, 0                    ! enc   

#ifdef __sparc_v9__
	be,pn	%icc, .ncbc.dec
#else
	be	.ncbc.dec
#endif
	STPTR	in4,  [%sp+BIAS+ARG0+4*ARGSZ] 

	! addr  left  right  temp  label
	

! load_little_endian
! in4 in5 out5 local3 .LLE1    

	! first in memory to rightmost in register

#ifdef __sparc_v9__
	andcc	in4, 3, global0
	bne,pn	%icc, .LLE1
	nop

	lda	[in4] 0x88, in5
	add	in4, 4, local3

	ba,pt	%icc, .LLE1a
	lda	[local3] 0x88, out5
#endif

.LLE1:
	ldub	[in4+3], in5

	ldub	[in4+2], local3
	sll	in5, 8, in5
	or	in5, local3, in5

	ldub	[in4+1], local3
	sll	in5, 8, in5
	or	in5, local3, in5

	ldub	[in4+0], local3
	sll	in5, 8, in5
	or	in5, local3, in5


	ldub	[in4+3+4], out5

	ldub	[in4+2+4], local3
	sll	out5, 8, out5
	or	out5, local3, out5

	ldub	[in4+1+4], local3
	sll	out5, 8, out5
	or	out5, local3, out5

	ldub	[in4+0+4], local3
	sll	out5, 8, out5
	or	out5, local3, out5
.LLE1a:

  ! iv

	addcc	in2, -8, in2              ! bytes missing when first block done

#ifdef __sparc_v9__
	bl,pn	%icc, .ncbc.enc.seven.or.less
#else
	bl	.ncbc.enc.seven.or.less
#endif
	mov	in3, in4                  ! schedule

.ncbc.enc.next.block:

	

! load_little_endian
! in0 out4 global4 local3 .LLE2    

	! first in memory to rightmost in register

#ifdef __sparc_v9__
	andcc	in0, 3, global0
	bne,pn	%icc, .LLE2
	nop

	lda	[in0] 0x88, out4
	add	in0, 4, local3

	ba,pt	%icc, .LLE2a
	lda	[local3] 0x88, global4
#endif

.LLE2:
	ldub	[in0+3], out4

	ldub	[in0+2], local3
	sll	out4, 8, out4
	or	out4, local3, out4

	ldub	[in0+1], local3
	sll	out4, 8, out4
	or	out4, local3, out4

	ldub	[in0+0], local3
	sll	out4, 8, out4
	or	out4, local3, out4


	ldub	[in0+3+4], global4

	ldub	[in0+2+4], local3
	sll	global4, 8, global4
	or	global4, local3, global4

	ldub	[in0+1+4], local3
	sll	global4, 8, global4
	or	global4, local3, global4

	ldub	[in0+0+4], local3
	sll	global4, 8, global4
	or	global4, local3, global4
.LLE2a:

  ! block

.ncbc.enc.next.block_1:

	xor	in5, out4, in5            ! iv xor
	xor	out5, global4, out5       ! iv xor

	! parameter 8  1 for move in3 to in4, 2 for move in4 to in3
	

! ip_macro
! in5 out5 out5 in5 in3 0 0 2 

	ld	[out2+256], local1
	srl	out5, 4, local4

	xor	local4, in5, local4
	nop

	ld	[out2+260], local2
	and	local4, local1, local4
	
	mov in4, in3

	ld	[out2+280], out4          ! loop counter
	sll	local4, 4, local1
	xor	in5, local4, in5

	ld	[out2+264], local3
	srl	in5, 16, local4
	xor	out5, local1, out5

	
	xor	local4, out5, local4
	nop	!sethi	%hi(DES_SPtrans), global1 ! sbox addr

	
	and	local4, local2, local4
	nop	!or	global1, %lo(DES_SPtrans), global1   ! sbox addr

	sll	local4, 16, local1
	xor	out5, local4, out5

	srl	out5, 2, local4
	xor	in5, local1, in5

	sethi	%hi(16711680), local5
	xor	local4, in5, local4

	and	local4, local3, local4
	or	local5, 255, local5

	sll	local4, 2, local2
	xor	in5, local4, in5

	srl	in5, 8, local4
	xor	out5, local2, out5

	xor	local4, out5, local4
	add	global1, 768, global4

	and	local4, local5, local4
	add	global1, 1024, global5

	ld	[out2+272], local7
	sll	local4, 8, local1
	xor	out5, local4, out5

	srl	out5, 1, local4
	xor	in5, local1, in5

	ld	[in3], out0                ! key 7531
	xor	local4, in5, local4
	add	global1, 256, global2

	ld	[in3+4], out1              ! key 8642
	and	local4, local7, local4
	add	global1, 512, global3

	sll	local4, 1, local1
	xor	in5, local4, in5

	sll	in5, 3, local3
	xor	out5, local1, out5

	sll	out5, 3, local2
	add	global1, 1280, local6     ! address sbox 8

	srl	in5, 29, local4
	add	global1, 1792, out3       ! address sbox 8

	srl	out5, 29, local1
	or	local4, local3, out5

	or	local2, local1, in5

	

	


.ncbc.enc.next.block_2:

!//	call .des_enc                     ! compares in2 to 8
!	rounds inlined for alignment purposes

	add	global1, 768, global4     ! address sbox 4 since register used below

	

! rounds_macro
! in5 out5 1 .ncbc.enc.1 in3 in4   

	xor	out5, out0, local1

	ld	[out2+284], local5        ! 0x0000FC00
	ba	.ncbc.enc.1
	and	local1, 252, local1

	.align 32

.ncbc.enc.1:
	! local6 is address sbox 6
	! out3   is address sbox 8
	! out4   is loop counter

	ld	[global1+local1], local1
	xor	out5, out1, out1            ! 8642
	xor	out5, out0, out0            ! 7531
	! fmovs	%f0, %f0                  ! fxor used for alignment

	srl	out1, 4, local0           ! rotate 4 right
	and	out0, local5, local3      ! 3
	! fmovs	%f0, %f0

	ld	[in3+1*8], local7         ! key 7531 next round
	srl	local3, 8, local3         ! 3
	and	local0, 252, local2       ! 2
	! fmovs	%f0, %f0

	ld	[global3+local3],local3   ! 3
	sll	out1, 28, out1            ! rotate
	xor	in5, local1, in5            ! 1 finished, local1 now sbox 7

	ld	[global2+local2], local2  ! 2 
	srl	out0, 24, local1          ! 7
	or	out1, local0, out1        ! rotate

	ldub	[out2+local1], local1     ! 7 (and 0xFC)
	srl	out1, 24, local0          ! 8
	and	out1, local5, local4      ! 4

	ldub	[out2+local0], local0     ! 8 (and 0xFC)
	srl	local4, 8, local4         ! 4
	xor	in5, local2, in5            ! 2 finished local2 now sbox 6

	ld	[global4+local4],local4   ! 4
	srl	out1, 16, local2          ! 6
	xor	in5, local3, in5            ! 3 finished local3 now sbox 5

	ld	[out3+local0],local0      ! 8
	and	local2, 252, local2       ! 6
	add	global1, 1536, local5     ! address sbox 7

	ld	[local6+local2], local2   ! 6
	srl	out0, 16, local3          ! 5
	xor	in5, local4, in5            ! 4 finished

	ld	[local5+local1],local1    ! 7
	and	local3, 252, local3       ! 5
	xor	in5, local0, in5            ! 8 finished

	ld	[global5+local3],local3   ! 5
	xor	in5, local2, in5            ! 6 finished
	subcc	out4, 1, out4

	ld	[in3+1*8+4], out0         ! key 8642 next round
	xor	in5, local7, local2        ! sbox 5 next round
	xor	in5, local1, in5            ! 7 finished

	srl	local2, 16, local2        ! sbox 5 next round
	xor	in5, local3, in5            ! 5 finished

	ld	[in3+1*16+4], out1        ! key 8642 next round again
	and	local2, 252, local2       ! sbox5 next round
! next round
	xor	in5, local7, local7        ! 7531

	ld	[global5+local2], local2  ! 5
	srl	local7, 24, local3        ! 7
	xor	in5, out0, out0            ! 8642

	ldub	[out2+local3], local3     ! 7 (and 0xFC)
	srl	out0, 4, local0           ! rotate 4 right
	and	local7, 252, local1       ! 1

	sll	out0, 28, out0            ! rotate
	xor	out5, local2, out5            ! 5 finished local2 used

	srl	local0, 8, local4         ! 4
	and	local0, 252, local2       ! 2
	ld	[local5+local3], local3   ! 7

	srl	local0, 16, local5        ! 6
	or	out0, local0, out0        ! rotate
	ld	[global2+local2], local2  ! 2

	srl	out0, 24, local0
	ld	[in3+1*16], out0          ! key 7531 next round
	and	local4, 252, local4	  ! 4

	and	local5, 252, local5       ! 6
	ld	[global4+local4], local4  ! 4
	xor	out5, local3, out5            ! 7 finished local3 used

	and	local0, 252, local0       ! 8
	ld	[local6+local5], local5   ! 6
	xor	out5, local2, out5            ! 2 finished local2 now sbox 3

	srl	local7, 8, local2         ! 3 start
	ld	[out3+local0], local0     ! 8
	xor	out5, local4, out5            ! 4 finished

	and	local2, 252, local2       ! 3
	ld	[global1+local1], local1  ! 1
	xor	out5, local5, out5            ! 6 finished local5 used

	ld	[global3+local2], local2  ! 3
	xor	out5, local0, out5            ! 8 finished
	add	in3, 1*16, in3             ! enc add 8, dec add -8 to key pointer

	ld	[out2+284], local5        ! 0x0000FC00
	xor	out5, out0, local4          ! sbox 1 next round
	xor	out5, local1, out5            ! 1 finished

	xor	out5, local2, out5            ! 3 finished
#ifdef __sparc_v9__
	bne,pt	%icc, .ncbc.enc.1
#else
	bne	.ncbc.enc.1
#endif
	and	local4, 252, local1       ! sbox 1 next round

! two rounds more:

	ld	[global1+local1], local1
	xor	out5, out1, out1
	xor	out5, out0, out0

	srl	out1, 4, local0           ! rotate
	and	out0, local5, local3

	ld	[in3+1*8], local7         ! key 7531
	srl	local3, 8, local3
	and	local0, 252, local2

	ld	[global3+local3],local3
	sll	out1, 28, out1            ! rotate
	xor	in5, local1, in5            ! 1 finished, local1 now sbox 7

	ld	[global2+local2], local2
	srl	out0, 24, local1
	or	out1, local0, out1        ! rotate

	ldub	[out2+local1], local1
	srl	out1, 24, local0
	and	out1, local5, local4

	ldub	[out2+local0], local0
	srl	local4, 8, local4
	xor	in5, local2, in5            ! 2 finished local2 now sbox 6

	ld	[global4+local4],local4
	srl	out1, 16, local2
	xor	in5, local3, in5            ! 3 finished local3 now sbox 5

	ld	[out3+local0],local0
	and	local2, 252, local2
	add	global1, 1536, local5     ! address sbox 7

	ld	[local6+local2], local2
	srl	out0, 16, local3
	xor	in5, local4, in5            ! 4 finished

	ld	[local5+local1],local1
	and	local3, 252, local3
	xor	in5, local0, in5

	ld	[global5+local3],local3
	xor	in5, local2, in5            ! 6 finished
	cmp	in2, 8

	ld	[out2+280], out4  ! loop counter
	xor	in5, local7, local2        ! sbox 5 next round
	xor	in5, local1, in5            ! 7 finished

	ld	[in3+1*8+4], out0
	srl	local2, 16, local2        ! sbox 5 next round
	xor	in5, local3, in5            ! 5 finished

	and	local2, 252, local2
! next round (two rounds more)
	xor	in5, local7, local7        ! 7531

	ld	[global5+local2], local2
	srl	local7, 24, local3
	xor	in5, out0, out0            ! 8642

	ldub	[out2+local3], local3
	srl	out0, 4, local0           ! rotate
	and	local7, 252, local1

	sll	out0, 28, out0            ! rotate
	xor	out5, local2, out5            ! 5 finished local2 used

	srl	local0, 8, local4
	and	local0, 252, local2
	ld	[local5+local3], local3

	srl	local0, 16, local5
	or	out0, local0, out0        ! rotate
	ld	[global2+local2], local2

	srl	out0, 24, local0
	ld	[in4], out0   ! key next encryption/decryption
	and	local4, 252, local4

	and	local5, 252, local5
	ld	[global4+local4], local4
	xor	out5, local3, out5            ! 7 finished local3 used

	and	local0, 252, local0
	ld	[local6+local5], local5
	xor	out5, local2, out5            ! 2 finished local2 now sbox 3

	srl	local7, 8, local2         ! 3 start
	ld	[out3+local0], local0
	xor	out5, local4, out5

	and	local2, 252, local2
	ld	[global1+local1], local1
	xor	out5, local5, out5            ! 6 finished local5 used

	ld	[global3+local2], local2
	srl	in5, 3, local3
	xor	out5, local0, out5

	ld	[in4+4], out1 ! key next encryption/decryption
	sll	in5, 29, local4
	xor	out5, local1, out5

	
	xor	out5, local2, out5
 ! include encryption  ks in3

#ifdef __sparc_v9__
	bl,pn	%icc, .ncbc.enc.next.block_fp
#else
	bl	.ncbc.enc.next.block_fp
#endif
	add	in0, 8, in0               ! input address

	! If 8 or more bytes are to be encrypted after this block,
	! we combine final permutation for this block with initial
	! permutation for next block. Load next block:

	

! load_little_endian
! in0 global3 global4 local5 .LLE12    

	! first in memory to rightmost in register

#ifdef __sparc_v9__
	andcc	in0, 3, global0
	bne,pn	%icc, .LLE12
	nop

	lda	[in0] 0x88, global3
	add	in0, 4, local5

	ba,pt	%icc, .LLE12a
	lda	[local5] 0x88, global4
#endif

.LLE12:
	ldub	[in0+3], global3

	ldub	[in0+2], local5
	sll	global3, 8, global3
	or	global3, local5, global3

	ldub	[in0+1], local5
	sll	global3, 8, global3
	or	global3, local5, global3

	ldub	[in0+0], local5
	sll	global3, 8, global3
	or	global3, local5, global3


	ldub	[in0+3+4], global4

	ldub	[in0+2+4], local5
	sll	global4, 8, global4
	or	global4, local5, global4

	ldub	[in0+1+4], local5
	sll	global4, 8, global4
	or	global4, local5, global4

	ldub	[in0+0+4], local5
	sll	global4, 8, global4
	or	global4, local5, global4
.LLE12a:



	!  parameter 1   original left
	!  parameter 2   original right
	!  parameter 3   left ip
	!  parameter 4   right ip
	!  parameter 5   1: load ks1/ks2 to in3/in4, add 120 to in4
	!                2: mov in4 to in3
	!
	! also adds -8 to length in2 and loads loop counter to out4

	

! fp_ip_macro
! out0 out1 global3 global4 2    

	
	

	
	
	
	

	! out0 in local3, local4

	ld	[out2+256], local1
	sll	out5, 29, out4
	or	local3, local4, out0

	srl	out5, 3, out1
	mov in4, in3

	ld	[out2+272], local5
	srl	global4, 4, local0
	or	out1, out4, out1

	srl	out1, 1, out4
	xor	out4, out0, out4

	and	out4, local5, out4
	xor	local0, global3, local0

	sll	out4, 1, local3
	xor	out0, out4, out0

	and	local0, local1, local0
	add	in2, -8, in2

	sll	local0, 4, local7
	xor	global3, local0, global3

	ld	[out2+268], local4
	srl	out0, 8, out4
	xor	out1, local3, out1
	ld	[out2+260], local2
	srl	global3, 16, local0
	xor	global4, local7, global4
	xor	out4, out1, out4
	xor	local0, global4, local0
	and	out4, local4, out4
	and	local0, local2, local0
	sll	out4, 8, local3
	xor	out1, out4, out1
	sll	local0, 16, local7
	xor	global4, local0, global4

	srl	out1, 2, out4
	xor	out0, local3, out0

	ld	[out2+264], local3         ! ip3
	srl	global4, 2, local0
	xor	global3, local7, global3
	xor	out4, out0, out4
	xor	local0, global3, local0
	and	out4, local3, out4
	and	local0, local3, local0
	sll	out4, 2, local3
	xor	out0, out4, out0
	sll	local0, 2, local7
	xor	global3, local0, global3

	srl	out0, 16, out4
	xor	out1, local3, out1
	srl	global3, 8, local0
	xor	global4, local7, global4
	xor	out4, out1, out4
	xor	local0, global4, local0
	and	out4, local2, out4
	and	local0, local4, local0
	sll	out4, 16, local3
	xor	out1, out4, local4
	sll	local0, 8, local7
	xor	global4, local0, global4

	srl	global4, 1, local0
	xor	global3, local7, global3

	srl	local4, 4, out4
	xor	local0, global3, local0

	xor	out0, local3, out0
	and	local0, local5, local0

	sll	local0, 1, local7
	xor	out4, out0, out4

	xor	global3, local0, global3
	xor	global4, local7, global4

	sll	global3, 3, local5
	and	out4, local1, out4

	sll	out4, 4, local3
	xor	out0, out4, out0

	
	sll	global4, 3, local2
	xor	local4, local3, out1

	! reload since used as temporar:

	ld	[out2+280], out4          ! loop counter

	srl	global3, 29, local0
	

	
	srl	global4, 29, local7

	or	local0, local5, global4
	or	local2, local7, global3



	

! store_little_endian
! in1 out0 out1 local3 .SLE10    

	! rightmost in register to first in memory

#ifdef __sparc_v9__
	andcc	in1, 3, global0
	bne,pn	%icc, .SLE10
	nop

	sta	out0, [in1] 0x88
	add	in1, 4, local3

	ba,pt	%icc, .SLE10a
	sta	out1, [local3] 0x88
#endif

.SLE10:
	and	out0, 255, local3
	stub	local3, [in1+0]

	srl	out0, 8, local3
	and	local3, 255, local3
	stub	local3, [in1+1]

	srl	out0, 16, local3
	and	local3, 255, local3
	stub	local3, [in1+2]

	srl	out0, 24, local3
	stub	local3, [in1+3]


	and	out1, 255, local3
	stub	local3, [in1+0+4]

	srl	out1, 8, local3
	and	local3, 255, local3
	stub	local3, [in1+1+4]

	srl	out1, 16, local3
	and	local3, 255, local3
	stub	local3, [in1+2+4]

	srl	out1, 24, local3
	stub	local3, [in1+3+4]

.SLE10a:

  ! block

	ld	[in3], out0               ! key 7531 first round next block
	mov 	in5, local1
	xor	global3, out5, in5        ! iv xor next block

	ld	[in3+4], out1             ! key 8642
	add	global1, 512, global3     ! address sbox 3 since register used
	xor	global4, local1, out5     ! iv xor next block

	ba	.ncbc.enc.next.block_2
	add	in1, 8, in1               ! output adress

.ncbc.enc.next.block_fp:

	

! fp_macro
! in5 out5       

	! initially undo the rotate 3 left done after initial permutation
	! original left is received shifted 3 right and 29 left in local3/4

	sll	out5, 29, local1
	or	local3, local4, in5

	srl	out5, 3, out5
	sethi	%hi(0x55555555), local2

	or	out5, local1, out5
	or	local2, %lo(0x55555555), local2

	srl	out5, 1, local3
	sethi	%hi(0x00ff00ff), local1
	xor	local3, in5, local3
	or	local1, %lo(0x00ff00ff), local1
	and	local3, local2, local3
	sethi	%hi(0x33333333), local4
	sll	local3, 1, local2

	xor	in5, local3, in5

	srl	in5, 8, local3
	xor	out5, local2, out5
	xor	local3, out5, local3
	or	local4, %lo(0x33333333), local4
	and	local3, local1, local3
	sethi	%hi(0x0000ffff), local1
	sll	local3, 8, local2

	xor	out5, local3, out5

	srl	out5, 2, local3
	xor	in5, local2, in5
	xor	local3, in5, local3
	or	local1, %lo(0x0000ffff), local1
	and	local3, local4, local3
	sethi	%hi(0x0f0f0f0f), local4
	sll	local3, 2, local2

	
	xor	in5, local3, in5

	
	srl	in5, 16, local3
	xor	out5, local2, out5
	xor	local3, out5, local3
	or	local4, %lo(0x0f0f0f0f), local4
	and	local3, local1, local3
	sll	local3, 16, local2

	xor	out5, local3, local1

	srl	local1, 4, local3
	xor	in5, local2, in5
	xor	local3, in5, local3
	and	local3, local4, local3
	sll	local3, 4, local2

	xor	in5, local3, in5

	! optional store:

	

	xor	local1, local2, out5

	



	

! store_little_endian
! in1 in5 out5 local3 .SLE1    

	! rightmost in register to first in memory

#ifdef __sparc_v9__
	andcc	in1, 3, global0
	bne,pn	%icc, .SLE1
	nop

	sta	in5, [in1] 0x88
	add	in1, 4, local3

	ba,pt	%icc, .SLE1a
	sta	out5, [local3] 0x88
#endif

.SLE1:
	and	in5, 255, local3
	stub	local3, [in1+0]

	srl	in5, 8, local3
	and	local3, 255, local3
	stub	local3, [in1+1]

	srl	in5, 16, local3
	and	local3, 255, local3
	stub	local3, [in1+2]

	srl	in5, 24, local3
	stub	local3, [in1+3]


	and	out5, 255, local3
	stub	local3, [in1+0+4]

	srl	out5, 8, local3
	and	local3, 255, local3
	stub	local3, [in1+1+4]

	srl	out5, 16, local3
	and	local3, 255, local3
	stub	local3, [in1+2+4]

	srl	out5, 24, local3
	stub	local3, [in1+3+4]

.SLE1a:

  ! block

	addcc   in2, -8, in2              ! bytes missing when next block done

#ifdef __sparc_v9__
	bpos,pt	%icc, .ncbc.enc.next.block  ! also jumps if 0
#else
	bpos	.ncbc.enc.next.block
#endif
	add	in1, 8, in1

.ncbc.enc.seven.or.less:

	cmp	in2, -8

#ifdef __sparc_v9__
	ble,pt	%icc, .ncbc.enc.finish
#else
	ble	.ncbc.enc.finish
#endif
	nop

	add	in2, 8, local1            ! bytes to load

	! addr, length, dest left, dest right, temp, local3, label, ret label
	

! load_n_bytes
! in0 local1 local2 local3 .LNB1 .ncbc.enc.next.block_1 .LNB1 .ncbc.enc.next.block_1 

.LNB1.0:	call	.+8
	sll	local1, 2, local3

	add	%o7,.LNB1.jmp.table-.LNB1.0,local2

	add	local2, local3, local2
	mov	0, out4

	ld	[local2], local2

	jmp	%o7+local2
	mov	0, global4

.LNB1.7:
	ldub	[in0+6], local2
	sll	local2, 16, local2
	or	global4, local2, global4
.LNB1.6:
	ldub	[in0+5], local2
	sll	local2, 8, local2
	or	global4, local2, global4
.LNB1.5:
	ldub	[in0+4], local2
	or	global4, local2, global4
.LNB1.4:
	ldub	[in0+3], local2
	sll	local2, 24, local2
	or	out4, local2, out4
.LNB1.3:
	ldub	[in0+2], local2
	sll	local2, 16, local2
	or	out4, local2, out4
.LNB1.2:
	ldub	[in0+1], local2
	sll	local2, 8, local2
	or	out4, local2, out4
.LNB1.1:
	ldub	[in0+0], local2
	ba	.ncbc.enc.next.block_1
	or	out4, local2, out4

	.align 4

.LNB1.jmp.table:
	.word	0
	.word	.LNB1.1-.LNB1.0
	.word	.LNB1.2-.LNB1.0
	.word	.LNB1.3-.LNB1.0
	.word	.LNB1.4-.LNB1.0
	.word	.LNB1.5-.LNB1.0
	.word	.LNB1.6-.LNB1.0
	.word	.LNB1.7-.LNB1.0


	! Loads 1 to 7 bytes little endian to global4, out4


.ncbc.enc.finish:

	LDPTR	 [%sp+BIAS+ARG0+4*ARGSZ] , local4
	

! store_little_endian
! local4 in5 out5 local5 .SLE2    

	! rightmost in register to first in memory

#ifdef __sparc_v9__
	andcc	local4, 3, global0
	bne,pn	%icc, .SLE2
	nop

	sta	in5, [local4] 0x88
	add	local4, 4, local5

	ba,pt	%icc, .SLE2a
	sta	out5, [local5] 0x88
#endif

.SLE2:
	and	in5, 255, local5
	stub	local5, [local4+0]

	srl	in5, 8, local5
	and	local5, 255, local5
	stub	local5, [local4+1]

	srl	in5, 16, local5
	and	local5, 255, local5
	stub	local5, [local4+2]

	srl	in5, 24, local5
	stub	local5, [local4+3]


	and	out5, 255, local5
	stub	local5, [local4+0+4]

	srl	out5, 8, local5
	and	local5, 255, local5
	stub	local5, [local4+1+4]

	srl	out5, 16, local5
	and	local5, 255, local5
	stub	local5, [local4+2+4]

	srl	out5, 24, local5
	stub	local5, [local4+3+4]

.SLE2a:

  ! ivec

	ret
	restore


.ncbc.dec:

	STPTR	in0,  [%sp+BIAS+ARG0+0*ARGSZ] 
	cmp	in2, 0                    ! length
	add	in3, 120, in3

	LDPTR	 [%sp+BIAS+ARG0+4*ARGSZ] , local7              ! ivec
#ifdef __sparc_v9__
	ble,pn	%icc, .ncbc.dec.finish
#else
	ble	.ncbc.dec.finish
#endif
	mov	in3, in4                  ! schedule

	STPTR	in1,  [%sp+BIAS+ARG0+1*ARGSZ] 
	mov	in0, local5               ! input

	

! load_little_endian
! local7 in0 in1 local3 .LLE3    

	! first in memory to rightmost in register

#ifdef __sparc_v9__
	andcc	local7, 3, global0
	bne,pn	%icc, .LLE3
	nop

	lda	[local7] 0x88, in0
	add	local7, 4, local3

	ba,pt	%icc, .LLE3a
	lda	[local3] 0x88, in1
#endif

.LLE3:
	ldub	[local7+3], in0

	ldub	[local7+2], local3
	sll	in0, 8, in0
	or	in0, local3, in0

	ldub	[local7+1], local3
	sll	in0, 8, in0
	or	in0, local3, in0

	ldub	[local7+0], local3
	sll	in0, 8, in0
	or	in0, local3, in0


	ldub	[local7+3+4], in1

	ldub	[local7+2+4], local3
	sll	in1, 8, in1
	or	in1, local3, in1

	ldub	[local7+1+4], local3
	sll	in1, 8, in1
	or	in1, local3, in1

	ldub	[local7+0+4], local3
	sll	in1, 8, in1
	or	in1, local3, in1
.LLE3a:

   ! ivec

.ncbc.dec.next.block:

	

! load_little_endian
! local5 in5 out5 local3 .LLE4    

	! first in memory to rightmost in register

#ifdef __sparc_v9__
	andcc	local5, 3, global0
	bne,pn	%icc, .LLE4
	nop

	lda	[local5] 0x88, in5
	add	local5, 4, local3

	ba,pt	%icc, .LLE4a
	lda	[local3] 0x88, out5
#endif

.LLE4:
	ldub	[local5+3], in5

	ldub	[local5+2], local3
	sll	in5, 8, in5
	or	in5, local3, in5

	ldub	[local5+1], local3
	sll	in5, 8, in5
	or	in5, local3, in5

	ldub	[local5+0], local3
	sll	in5, 8, in5
	or	in5, local3, in5


	ldub	[local5+3+4], out5

	ldub	[local5+2+4], local3
	sll	out5, 8, out5
	or	out5, local3, out5

	ldub	[local5+1+4], local3
	sll	out5, 8, out5
	or	out5, local3, out5

	ldub	[local5+0+4], local3
	sll	out5, 8, out5
	or	out5, local3, out5
.LLE4a:

  ! block

	! parameter 6  1/2 for include encryption/decryption
	! parameter 7  1 for mov in1 to in3
	! parameter 8  1 for mov in3 to in4

	

! ip_macro
! in5 out5 in5 out5 in4 2 0 1 

	ld	[out2+256], local1
	srl	out5, 4, local4

	xor	local4, in5, local4
	nop

	ld	[out2+260], local2
	and	local4, local1, local4
	mov in3, in4
	

	ld	[out2+280], out4          ! loop counter
	sll	local4, 4, local1
	xor	in5, local4, in5

	ld	[out2+264], local3
	srl	in5, 16, local4
	xor	out5, local1, out5

	
	xor	local4, out5, local4
	nop	!sethi	%hi(DES_SPtrans), global1 ! sbox addr

	
	and	local4, local2, local4
	nop	!or	global1, %lo(DES_SPtrans), global1   ! sbox addr

	sll	local4, 16, local1
	xor	out5, local4, out5

	srl	out5, 2, local4
	xor	in5, local1, in5

	sethi	%hi(16711680), local5
	xor	local4, in5, local4

	and	local4, local3, local4
	or	local5, 255, local5

	sll	local4, 2, local2
	xor	in5, local4, in5

	srl	in5, 8, local4
	xor	out5, local2, out5

	xor	local4, out5, local4
	add	global1, 768, global4

	and	local4, local5, local4
	add	global1, 1024, global5

	ld	[out2+272], local7
	sll	local4, 8, local1
	xor	out5, local4, out5

	srl	out5, 1, local4
	xor	in5, local1, in5

	ld	[in4], out0                ! key 7531
	xor	local4, in5, local4
	add	global1, 256, global2

	ld	[in4+4], out1              ! key 8642
	and	local4, local7, local4
	add	global1, 512, global3

	sll	local4, 1, local1
	xor	in5, local4, in5

	sll	in5, 3, local3
	xor	out5, local1, out5

	sll	out5, 3, local2
	add	global1, 1280, local6     ! address sbox 8

	srl	in5, 29, local4
	add	global1, 1792, out3       ! address sbox 8

	srl	out5, 29, local1
	or	local4, local3, in5

	or	local2, local1, out5

	

	

		ld	[out2+284], local5     ! 0x0000FC00 used in the rounds
		or	local2, local1, out5
		xor	in5, out0, local1

		call .des_dec.1
		and	local1, 252, local1

	
 ! include decryprion  ks in4

	

! fp_macro
! out5 in5 0 1     

	! initially undo the rotate 3 left done after initial permutation
	! original left is received shifted 3 right and 29 left in local3/4

	sll	in5, 29, local1
	or	local3, local4, out5

	srl	in5, 3, in5
	sethi	%hi(0x55555555), local2

	or	in5, local1, in5
	or	local2, %lo(0x55555555), local2

	srl	in5, 1, local3
	sethi	%hi(0x00ff00ff), local1
	xor	local3, out5, local3
	or	local1, %lo(0x00ff00ff), local1
	and	local3, local2, local3
	sethi	%hi(0x33333333), local4
	sll	local3, 1, local2

	xor	out5, local3, out5

	srl	out5, 8, local3
	xor	in5, local2, in5
	xor	local3, in5, local3
	or	local4, %lo(0x33333333), local4
	and	local3, local1, local3
	sethi	%hi(0x0000ffff), local1
	sll	local3, 8, local2

	xor	in5, local3, in5

	srl	in5, 2, local3
	xor	out5, local2, out5
	xor	local3, out5, local3
	or	local1, %lo(0x0000ffff), local1
	and	local3, local4, local3
	sethi	%hi(0x0f0f0f0f), local4
	sll	local3, 2, local2

	LDPTR  [%sp+BIAS+ARG0+0*ARGSZ] , local5
	xor	out5, local3, out5

	LDPTR  [%sp+BIAS+ARG0+1*ARGSZ] , local7
	srl	out5, 16, local3
	xor	in5, local2, in5
	xor	local3, in5, local3
	or	local4, %lo(0x0f0f0f0f), local4
	and	local3, local1, local3
	sll	local3, 16, local2

	xor	in5, local3, local1

	srl	local1, 4, local3
	xor	out5, local2, out5
	xor	local3, out5, local3
	and	local3, local4, local3
	sll	local3, 4, local2

	xor	out5, local3, out5

	! optional store:

	

	xor	local1, local2, in5

	

 ! 1 for input and output address to local5/7

	! in2 is bytes left to be stored
	! in2 is compared to 8 in the rounds

	xor	out5, in0, out4           ! iv xor
#ifdef __sparc_v9__
	bl,pn	%icc, .ncbc.dec.seven.or.less
#else
	bl	.ncbc.dec.seven.or.less
#endif
	xor	in5, in1, global4         ! iv xor

	! Load ivec next block now, since input and output address might be the same.

	

! load_little_endian_inc
! local5 in0 in1 local3 .LLE5    

	! first in memory to rightmost in register

#ifdef __sparc_v9__
	andcc	local5, 3, global0
	bne,pn	%icc, .LLE5
	nop

	lda	[local5] 0x88, in0
	add	local5, 4, local5

	lda	[local5] 0x88, in1
	ba,pt	%icc, .LLE5a
	add	local5, 4, local5
#endif

.LLE5:
	ldub	[local5+3], in0

	ldub	[local5+2], local3
	sll	in0, 8, in0
	or	in0, local3, in0

	ldub	[local5+1], local3
	sll	in0, 8, in0
	or	in0, local3, in0

	ldub	[local5+0], local3
	sll	in0, 8, in0
	or	in0, local3, in0

	ldub	[local5+3+4], in1
	add	local5, 8, local5

	ldub	[local5+2+4-8], local3
	sll	in1, 8, in1
	or	in1, local3, in1

	ldub	[local5+1+4-8], local3
	sll	in1, 8, in1
	or	in1, local3, in1

	ldub	[local5+0+4-8], local3
	sll	in1, 8, in1
	or	in1, local3, in1
.LLE5a:

  ! iv

	

! store_little_endian
! local7 out4 global4 local3 .SLE3    

	! rightmost in register to first in memory

#ifdef __sparc_v9__
	andcc	local7, 3, global0
	bne,pn	%icc, .SLE3
	nop

	sta	out4, [local7] 0x88
	add	local7, 4, local3

	ba,pt	%icc, .SLE3a
	sta	global4, [local3] 0x88
#endif

.SLE3:
	and	out4, 255, local3
	stub	local3, [local7+0]

	srl	out4, 8, local3
	and	local3, 255, local3
	stub	local3, [local7+1]

	srl	out4, 16, local3
	and	local3, 255, local3
	stub	local3, [local7+2]

	srl	out4, 24, local3
	stub	local3, [local7+3]


	and	global4, 255, local3
	stub	local3, [local7+0+4]

	srl	global4, 8, local3
	and	local3, 255, local3
	stub	local3, [local7+1+4]

	srl	global4, 16, local3
	and	local3, 255, local3
	stub	local3, [local7+2+4]

	srl	global4, 24, local3
	stub	local3, [local7+3+4]

.SLE3a:



	STPTR	local5,  [%sp+BIAS+ARG0+0*ARGSZ] 
	add	local7, 8, local7
	addcc   in2, -8, in2

#ifdef __sparc_v9__
	bg,pt	%icc, .ncbc.dec.next.block
#else
	bg	.ncbc.dec.next.block
#endif
	STPTR	local7,  [%sp+BIAS+ARG0+1*ARGSZ] 


.ncbc.dec.store.iv:

	LDPTR	 [%sp+BIAS+ARG0+4*ARGSZ] , local4              ! ivec
	

! store_little_endian
! local4 in0 in1 local5 .SLE4    

	! rightmost in register to first in memory

#ifdef __sparc_v9__
	andcc	local4, 3, global0
	bne,pn	%icc, .SLE4
	nop

	sta	in0, [local4] 0x88
	add	local4, 4, local5

	ba,pt	%icc, .SLE4a
	sta	in1, [local5] 0x88
#endif

.SLE4:
	and	in0, 255, local5
	stub	local5, [local4+0]

	srl	in0, 8, local5
	and	local5, 255, local5
	stub	local5, [local4+1]

	srl	in0, 16, local5
	and	local5, 255, local5
	stub	local5, [local4+2]

	srl	in0, 24, local5
	stub	local5, [local4+3]


	and	in1, 255, local5
	stub	local5, [local4+0+4]

	srl	in1, 8, local5
	and	local5, 255, local5
	stub	local5, [local4+1+4]

	srl	in1, 16, local5
	and	local5, 255, local5
	stub	local5, [local4+2+4]

	srl	in1, 24, local5
	stub	local5, [local4+3+4]

.SLE4a:



.ncbc.dec.finish:

	ret
	restore

.ncbc.dec.seven.or.less:

	

! load_little_endian_inc
! local5 in0 in1 local3 .LLE13    

	! first in memory to rightmost in register

#ifdef __sparc_v9__
	andcc	local5, 3, global0
	bne,pn	%icc, .LLE13
	nop

	lda	[local5] 0x88, in0
	add	local5, 4, local5

	lda	[local5] 0x88, in1
	ba,pt	%icc, .LLE13a
	add	local5, 4, local5
#endif

.LLE13:
	ldub	[local5+3], in0

	ldub	[local5+2], local3
	sll	in0, 8, in0
	or	in0, local3, in0

	ldub	[local5+1], local3
	sll	in0, 8, in0
	or	in0, local3, in0

	ldub	[local5+0], local3
	sll	in0, 8, in0
	or	in0, local3, in0

	ldub	[local5+3+4], in1
	add	local5, 8, local5

	ldub	[local5+2+4-8], local3
	sll	in1, 8, in1
	or	in1, local3, in1

	ldub	[local5+1+4-8], local3
	sll	in1, 8, in1
	or	in1, local3, in1

	ldub	[local5+0+4-8], local3
	sll	in1, 8, in1
	or	in1, local3, in1
.LLE13a:

     ! ivec

	

! store_n_bytes
! local7 in2 local3 local4 .SNB1 .ncbc.dec.store.iv .SNB1 .ncbc.dec.store.iv 

.SNB1.0:	call	.+8
	sll	in2, 2, local4

	add	%o7,.SNB1.jmp.table-.SNB1.0,local3

	add	local3, local4, local3

	ld	[local3], local3

	jmp	%o7+local3
	nop

.SNB1.7:
	srl	global4, 16, local3
	and	local3, 0xff, local3
	stub	local3, [local7+6]
.SNB1.6:
	srl	global4, 8, local3
	and	local3, 0xff, local3
	stub	local3, [local7+5]
.SNB1.5:
	and	global4, 0xff, local3
	stub	local3, [local7+4]
.SNB1.4:
	srl	out4, 24, local3
	stub	local3, [local7+3]
.SNB1.3:
	srl	out4, 16, local3
	and	local3, 0xff, local3
	stub	local3, [local7+2]
.SNB1.2:
	srl	out4, 8, local3
	and	local3, 0xff, local3
	stub	local3, [local7+1]
.SNB1.1:
	and	out4, 0xff, local3


	ba	.ncbc.dec.store.iv
	stub	local3, [local7]

	.align 4

.SNB1.jmp.table:

	.word	0
	.word	.SNB1.1-.SNB1.0
	.word	.SNB1.2-.SNB1.0
	.word	.SNB1.3-.SNB1.0
	.word	.SNB1.4-.SNB1.0
	.word	.SNB1.5-.SNB1.0
	.word	.SNB1.6-.SNB1.0
	.word	.SNB1.7-.SNB1.0



.DES_ncbc_encrypt.end:
	.size	 DES_ncbc_encrypt, .DES_ncbc_encrypt.end-DES_ncbc_encrypt


! void DES_ede3_cbc_encrypt(input, output, lenght, ks1, ks2, ks3, ivec, enc)
! **************************************************************************


	.align 32
	.global DES_ede3_cbc_encrypt
	.type	 DES_ede3_cbc_encrypt,#function

DES_ede3_cbc_encrypt:

	save	%sp, FRAME, %sp

	
	
	

	sethi	%hi(_PIC_DES_SPtrans-1f),global1
	or	global1,%lo(_PIC_DES_SPtrans-1f),global1
1:	call	.+8
	add	%o7,global1,global1
	sub	global1,_PIC_DES_SPtrans-.des_and,out2

	LDPTR	[%fp+BIAS+ARG0+7*ARGSZ], local3          ! enc
	LDPTR	[%fp+BIAS+ARG0+6*ARGSZ], local4          ! ivec
	cmp	local3, 0                 ! enc

#ifdef __sparc_v9__
	be,pn	%icc, .ede3.dec
#else
	be	.ede3.dec
#endif
	STPTR	in4,  [%sp+BIAS+ARG0+4*ARGSZ] 

	STPTR	in5,  [%sp+BIAS+ARG0+5*ARGSZ] 

	

! load_little_endian
! local4 in5 out5 local3 .LLE6    

	! first in memory to rightmost in register

#ifdef __sparc_v9__
	andcc	local4, 3, global0
	bne,pn	%icc, .LLE6
	nop

	lda	[local4] 0x88, in5
	add	local4, 4, local3

	ba,pt	%icc, .LLE6a
	lda	[local3] 0x88, out5
#endif

.LLE6:
	ldub	[local4+3], in5

	ldub	[local4+2], local3
	sll	in5, 8, in5
	or	in5, local3, in5

	ldub	[local4+1], local3
	sll	in5, 8, in5
	or	in5, local3, in5

	ldub	[local4+0], local3
	sll	in5, 8, in5
	or	in5, local3, in5


	ldub	[local4+3+4], out5

	ldub	[local4+2+4], local3
	sll	out5, 8, out5
	or	out5, local3, out5

	ldub	[local4+1+4], local3
	sll	out5, 8, out5
	or	out5, local3, out5

	ldub	[local4+0+4], local3
	sll	out5, 8, out5
	or	out5, local3, out5
.LLE6a:

  ! ivec

	addcc	in2, -8, in2              ! bytes missing after next block

#ifdef __sparc_v9__
	bl,pn	%icc,  .ede3.enc.seven.or.less
#else
	bl	.ede3.enc.seven.or.less
#endif
	STPTR	in3,  [%sp+BIAS+ARG0+3*ARGSZ] 

.ede3.enc.next.block:

	

! load_little_endian
! in0 out4 global4 local3 .LLE7    

	! first in memory to rightmost in register

#ifdef __sparc_v9__
	andcc	in0, 3, global0
	bne,pn	%icc, .LLE7
	nop

	lda	[in0] 0x88, out4
	add	in0, 4, local3

	ba,pt	%icc, .LLE7a
	lda	[local3] 0x88, global4
#endif

.LLE7:
	ldub	[in0+3], out4

	ldub	[in0+2], local3
	sll	out4, 8, out4
	or	out4, local3, out4

	ldub	[in0+1], local3
	sll	out4, 8, out4
	or	out4, local3, out4

	ldub	[in0+0], local3
	sll	out4, 8, out4
	or	out4, local3, out4


	ldub	[in0+3+4], global4

	ldub	[in0+2+4], local3
	sll	global4, 8, global4
	or	global4, local3, global4

	ldub	[in0+1+4], local3
	sll	global4, 8, global4
	or	global4, local3, global4

	ldub	[in0+0+4], local3
	sll	global4, 8, global4
	or	global4, local3, global4
.LLE7a:



.ede3.enc.next.block_1:

	LDPTR	 [%sp+BIAS+ARG0+4*ARGSZ] , in4
	xor	in5, out4, in5            ! iv xor
	xor	out5, global4, out5       ! iv xor

	LDPTR	 [%sp+BIAS+ARG0+3*ARGSZ] , in3
	add	in4, 120, in4             ! for decryption we use last subkey first
	nop

	

! ip_macro
! in5 out5 out5 in5 in3    

	ld	[out2+256], local1
	srl	out5, 4, local4

	xor	local4, in5, local4
	nop

	ld	[out2+260], local2
	and	local4, local1, local4
	
	

	ld	[out2+280], out4          ! loop counter
	sll	local4, 4, local1
	xor	in5, local4, in5

	ld	[out2+264], local3
	srl	in5, 16, local4
	xor	out5, local1, out5

	
	xor	local4, out5, local4
	nop	!sethi	%hi(DES_SPtrans), global1 ! sbox addr

	
	and	local4, local2, local4
	nop	!or	global1, %lo(DES_SPtrans), global1   ! sbox addr

	sll	local4, 16, local1
	xor	out5, local4, out5

	srl	out5, 2, local4
	xor	in5, local1, in5

	sethi	%hi(16711680), local5
	xor	local4, in5, local4

	and	local4, local3, local4
	or	local5, 255, local5

	sll	local4, 2, local2
	xor	in5, local4, in5

	srl	in5, 8, local4
	xor	out5, local2, out5

	xor	local4, out5, local4
	add	global1, 768, global4

	and	local4, local5, local4
	add	global1, 1024, global5

	ld	[out2+272], local7
	sll	local4, 8, local1
	xor	out5, local4, out5

	srl	out5, 1, local4
	xor	in5, local1, in5

	ld	[in3], out0                ! key 7531
	xor	local4, in5, local4
	add	global1, 256, global2

	ld	[in3+4], out1              ! key 8642
	and	local4, local7, local4
	add	global1, 512, global3

	sll	local4, 1, local1
	xor	in5, local4, in5

	sll	in5, 3, local3
	xor	out5, local1, out5

	sll	out5, 3, local2
	add	global1, 1280, local6     ! address sbox 8

	srl	in5, 29, local4
	add	global1, 1792, out3       ! address sbox 8

	srl	out5, 29, local1
	or	local4, local3, out5

	or	local2, local1, in5

	

	


.ede3.enc.next.block_2:

	call .des_enc                     ! ks1 in3
	nop

	call .des_dec                     ! ks2 in4
	LDPTR	 [%sp+BIAS+ARG0+5*ARGSZ] , in3

	call .des_enc                     ! ks3 in3  compares in2 to 8
	nop

#ifdef __sparc_v9__
	bl,pn	%icc, .ede3.enc.next.block_fp
#else
	bl	.ede3.enc.next.block_fp
#endif
	add	in0, 8, in0

	! If 8 or more bytes are to be encrypted after this block,
	! we combine final permutation for this block with initial
	! permutation for next block. Load next block:

	

! load_little_endian
! in0 global3 global4 local5 .LLE11    

	! first in memory to rightmost in register

#ifdef __sparc_v9__
	andcc	in0, 3, global0
	bne,pn	%icc, .LLE11
	nop

	lda	[in0] 0x88, global3
	add	in0, 4, local5

	ba,pt	%icc, .LLE11a
	lda	[local5] 0x88, global4
#endif

.LLE11:
	ldub	[in0+3], global3

	ldub	[in0+2], local5
	sll	global3, 8, global3
	or	global3, local5, global3

	ldub	[in0+1], local5
	sll	global3, 8, global3
	or	global3, local5, global3

	ldub	[in0+0], local5
	sll	global3, 8, global3
	or	global3, local5, global3


	ldub	[in0+3+4], global4

	ldub	[in0+2+4], local5
	sll	global4, 8, global4
	or	global4, local5, global4

	ldub	[in0+1+4], local5
	sll	global4, 8, global4
	or	global4, local5, global4

	ldub	[in0+0+4], local5
	sll	global4, 8, global4
	or	global4, local5, global4
.LLE11a:



	!  parameter 1   original left
	!  parameter 2   original right
	!  parameter 3   left ip
	!  parameter 4   right ip
	!  parameter 5   1: load ks1/ks2 to in3/in4, add 120 to in4
	!                2: mov in4 to in3
	!
	! also adds -8 to length in2 and loads loop counter to out4

	

! fp_ip_macro
! out0 out1 global3 global4 1    

	
	

	
	
	
	

	! out0 in local3, local4

	ld	[out2+256], local1
	sll	out5, 29, out4
	or	local3, local4, out0

	srl	out5, 3, out1
	

	ld	[out2+272], local5
	srl	global4, 4, local0
	or	out1, out4, out1

	srl	out1, 1, out4
	xor	out4, out0, out4

	and	out4, local5, out4
	xor	local0, global3, local0

	sll	out4, 1, local3
	xor	out0, out4, out0

	and	local0, local1, local0
	add	in2, -8, in2

	sll	local0, 4, local7
	xor	global3, local0, global3

	ld	[out2+268], local4
	srl	out0, 8, out4
	xor	out1, local3, out1
	ld	[out2+260], local2
	srl	global3, 16, local0
	xor	global4, local7, global4
	xor	out4, out1, out4
	xor	local0, global4, local0
	and	out4, local4, out4
	and	local0, local2, local0
	sll	out4, 8, local3
	xor	out1, out4, out1
	sll	local0, 16, local7
	xor	global4, local0, global4

	srl	out1, 2, out4
	xor	out0, local3, out0

	ld	[out2+264], local3         ! ip3
	srl	global4, 2, local0
	xor	global3, local7, global3
	xor	out4, out0, out4
	xor	local0, global3, local0
	and	out4, local3, out4
	and	local0, local3, local0
	sll	out4, 2, local3
	xor	out0, out4, out0
	sll	local0, 2, local7
	xor	global3, local0, global3

	srl	out0, 16, out4
	xor	out1, local3, out1
	srl	global3, 8, local0
	xor	global4, local7, global4
	xor	out4, out1, out4
	xor	local0, global4, local0
	and	out4, local2, out4
	and	local0, local4, local0
	sll	out4, 16, local3
	xor	out1, out4, local4
	sll	local0, 8, local7
	xor	global4, local0, global4

	srl	global4, 1, local0
	xor	global3, local7, global3

	srl	local4, 4, out4
	xor	local0, global3, local0

	xor	out0, local3, out0
	and	local0, local5, local0

	sll	local0, 1, local7
	xor	out4, out0, out4

	xor	global3, local0, global3
	xor	global4, local7, global4

	sll	global3, 3, local5
	and	out4, local1, out4

	sll	out4, 4, local3
	xor	out0, out4, out0

	LDPTR	 [%sp+BIAS+ARG0+4*ARGSZ] , in4
	sll	global4, 3, local2
	xor	local4, local3, out1

	! reload since used as temporar:

	ld	[out2+280], out4          ! loop counter

	srl	global3, 29, local0
	add in4, 120, in4

	LDPTR	 [%sp+BIAS+ARG0+3*ARGSZ] , in3
	srl	global4, 29, local7

	or	local0, local5, global4
	or	local2, local7, global3



	

! store_little_endian
! in1 out0 out1 local3 .SLE9    

	! rightmost in register to first in memory

#ifdef __sparc_v9__
	andcc	in1, 3, global0
	bne,pn	%icc, .SLE9
	nop

	sta	out0, [in1] 0x88
	add	in1, 4, local3

	ba,pt	%icc, .SLE9a
	sta	out1, [local3] 0x88
#endif

.SLE9:
	and	out0, 255, local3
	stub	local3, [in1+0]

	srl	out0, 8, local3
	and	local3, 255, local3
	stub	local3, [in1+1]

	srl	out0, 16, local3
	and	local3, 255, local3
	stub	local3, [in1+2]

	srl	out0, 24, local3
	stub	local3, [in1+3]


	and	out1, 255, local3
	stub	local3, [in1+0+4]

	srl	out1, 8, local3
	and	local3, 255, local3
	stub	local3, [in1+1+4]

	srl	out1, 16, local3
	and	local3, 255, local3
	stub	local3, [in1+2+4]

	srl	out1, 24, local3
	stub	local3, [in1+3+4]

.SLE9a:

  ! block

	mov 	in5, local1
	xor	global3, out5, in5        ! iv xor next block

	ld	[in3], out0               ! key 7531
	add	global1, 512, global3     ! address sbox 3
	xor	global4, local1, out5     ! iv xor next block

	ld	[in3+4], out1             ! key 8642
	add	global1, 768, global4     ! address sbox 4
	ba	.ede3.enc.next.block_2
	add	in1, 8, in1

.ede3.enc.next.block_fp:

	

! fp_macro
! in5 out5       

	! initially undo the rotate 3 left done after initial permutation
	! original left is received shifted 3 right and 29 left in local3/4

	sll	out5, 29, local1
	or	local3, local4, in5

	srl	out5, 3, out5
	sethi	%hi(0x55555555), local2

	or	out5, local1, out5
	or	local2, %lo(0x55555555), local2

	srl	out5, 1, local3
	sethi	%hi(0x00ff00ff), local1
	xor	local3, in5, local3
	or	local1, %lo(0x00ff00ff), local1
	and	local3, local2, local3
	sethi	%hi(0x33333333), local4
	sll	local3, 1, local2

	xor	in5, local3, in5

	srl	in5, 8, local3
	xor	out5, local2, out5
	xor	local3, out5, local3
	or	local4, %lo(0x33333333), local4
	and	local3, local1, local3
	sethi	%hi(0x0000ffff), local1
	sll	local3, 8, local2

	xor	out5, local3, out5

	srl	out5, 2, local3
	xor	in5, local2, in5
	xor	local3, in5, local3
	or	local1, %lo(0x0000ffff), local1
	and	local3, local4, local3
	sethi	%hi(0x0f0f0f0f), local4
	sll	local3, 2, local2

	
	xor	in5, local3, in5

	
	srl	in5, 16, local3
	xor	out5, local2, out5
	xor	local3, out5, local3
	or	local4, %lo(0x0f0f0f0f), local4
	and	local3, local1, local3
	sll	local3, 16, local2

	xor	out5, local3, local1

	srl	local1, 4, local3
	xor	in5, local2, in5
	xor	local3, in5, local3
	and	local3, local4, local3
	sll	local3, 4, local2

	xor	in5, local3, in5

	! optional store:

	

	xor	local1, local2, out5

	



	

! store_little_endian
! in1 in5 out5 local3 .SLE5    

	! rightmost in register to first in memory

#ifdef __sparc_v9__
	andcc	in1, 3, global0
	bne,pn	%icc, .SLE5
	nop

	sta	in5, [in1] 0x88
	add	in1, 4, local3

	ba,pt	%icc, .SLE5a
	sta	out5, [local3] 0x88
#endif

.SLE5:
	and	in5, 255, local3
	stub	local3, [in1+0]

	srl	in5, 8, local3
	and	local3, 255, local3
	stub	local3, [in1+1]

	srl	in5, 16, local3
	and	local3, 255, local3
	stub	local3, [in1+2]

	srl	in5, 24, local3
	stub	local3, [in1+3]


	and	out5, 255, local3
	stub	local3, [in1+0+4]

	srl	out5, 8, local3
	and	local3, 255, local3
	stub	local3, [in1+1+4]

	srl	out5, 16, local3
	and	local3, 255, local3
	stub	local3, [in1+2+4]

	srl	out5, 24, local3
	stub	local3, [in1+3+4]

.SLE5a:

  ! block

	addcc   in2, -8, in2              ! bytes missing when next block done

#ifdef __sparc_v9__
	bpos,pt	%icc, .ede3.enc.next.block
#else
	bpos	.ede3.enc.next.block
#endif
	add	in1, 8, in1

.ede3.enc.seven.or.less:

	cmp	in2, -8

#ifdef __sparc_v9__
	ble,pt	%icc, .ede3.enc.finish
#else
	ble	.ede3.enc.finish
#endif
	nop

	add	in2, 8, local1            ! bytes to load

	! addr, length, dest left, dest right, temp, local3, label, ret label
	

! load_n_bytes
! in0 local1 local2 local3 .LNB2 .ede3.enc.next.block_1 .LNB2 .ede3.enc.next.block_1 

.LNB2.0:	call	.+8
	sll	local1, 2, local3

	add	%o7,.LNB2.jmp.table-.LNB2.0,local2

	add	local2, local3, local2
	mov	0, out4

	ld	[local2], local2

	jmp	%o7+local2
	mov	0, global4

.LNB2.7:
	ldub	[in0+6], local2
	sll	local2, 16, local2
	or	global4, local2, global4
.LNB2.6:
	ldub	[in0+5], local2
	sll	local2, 8, local2
	or	global4, local2, global4
.LNB2.5:
	ldub	[in0+4], local2
	or	global4, local2, global4
.LNB2.4:
	ldub	[in0+3], local2
	sll	local2, 24, local2
	or	out4, local2, out4
.LNB2.3:
	ldub	[in0+2], local2
	sll	local2, 16, local2
	or	out4, local2, out4
.LNB2.2:
	ldub	[in0+1], local2
	sll	local2, 8, local2
	or	out4, local2, out4
.LNB2.1:
	ldub	[in0+0], local2
	ba	.ede3.enc.next.block_1
	or	out4, local2, out4

	.align 4

.LNB2.jmp.table:
	.word	0
	.word	.LNB2.1-.LNB2.0
	.word	.LNB2.2-.LNB2.0
	.word	.LNB2.3-.LNB2.0
	.word	.LNB2.4-.LNB2.0
	.word	.LNB2.5-.LNB2.0
	.word	.LNB2.6-.LNB2.0
	.word	.LNB2.7-.LNB2.0


.ede3.enc.finish:

	LDPTR	[%fp+BIAS+ARG0+6*ARGSZ], local4          ! ivec
	

! store_little_endian
! local4 in5 out5 local5 .SLE6    

	! rightmost in register to first in memory

#ifdef __sparc_v9__
	andcc	local4, 3, global0
	bne,pn	%icc, .SLE6
	nop

	sta	in5, [local4] 0x88
	add	local4, 4, local5

	ba,pt	%icc, .SLE6a
	sta	out5, [local5] 0x88
#endif

.SLE6:
	and	in5, 255, local5
	stub	local5, [local4+0]

	srl	in5, 8, local5
	and	local5, 255, local5
	stub	local5, [local4+1]

	srl	in5, 16, local5
	and	local5, 255, local5
	stub	local5, [local4+2]

	srl	in5, 24, local5
	stub	local5, [local4+3]


	and	out5, 255, local5
	stub	local5, [local4+0+4]

	srl	out5, 8, local5
	and	local5, 255, local5
	stub	local5, [local4+1+4]

	srl	out5, 16, local5
	and	local5, 255, local5
	stub	local5, [local4+2+4]

	srl	out5, 24, local5
	stub	local5, [local4+3+4]

.SLE6a:

  ! ivec

	ret
	restore

.ede3.dec:

	STPTR	in0,  [%sp+BIAS+ARG0+0*ARGSZ] 
	add	in5, 120, in5

	STPTR	in1,  [%sp+BIAS+ARG0+1*ARGSZ] 
	mov	in0, local5
	add	in3, 120, in3

	STPTR	in3,  [%sp+BIAS+ARG0+3*ARGSZ] 
	cmp	in2, 0

#ifdef __sparc_v9__
	ble	%icc, .ede3.dec.finish
#else
	ble	.ede3.dec.finish
#endif
	STPTR	in5,  [%sp+BIAS+ARG0+5*ARGSZ] 

	LDPTR	[%fp+BIAS+ARG0+6*ARGSZ], local7          ! iv
	

! load_little_endian
! local7 in0 in1 local3 .LLE8    

	! first in memory to rightmost in register

#ifdef __sparc_v9__
	andcc	local7, 3, global0
	bne,pn	%icc, .LLE8
	nop

	lda	[local7] 0x88, in0
	add	local7, 4, local3

	ba,pt	%icc, .LLE8a
	lda	[local3] 0x88, in1
#endif

.LLE8:
	ldub	[local7+3], in0

	ldub	[local7+2], local3
	sll	in0, 8, in0
	or	in0, local3, in0

	ldub	[local7+1], local3
	sll	in0, 8, in0
	or	in0, local3, in0

	ldub	[local7+0], local3
	sll	in0, 8, in0
	or	in0, local3, in0


	ldub	[local7+3+4], in1

	ldub	[local7+2+4], local3
	sll	in1, 8, in1
	or	in1, local3, in1

	ldub	[local7+1+4], local3
	sll	in1, 8, in1
	or	in1, local3, in1

	ldub	[local7+0+4], local3
	sll	in1, 8, in1
	or	in1, local3, in1
.LLE8a:



.ede3.dec.next.block:

	

! load_little_endian
! local5 in5 out5 local3 .LLE9    

	! first in memory to rightmost in register

#ifdef __sparc_v9__
	andcc	local5, 3, global0
	bne,pn	%icc, .LLE9
	nop

	lda	[local5] 0x88, in5
	add	local5, 4, local3

	ba,pt	%icc, .LLE9a
	lda	[local3] 0x88, out5
#endif

.LLE9:
	ldub	[local5+3], in5

	ldub	[local5+2], local3
	sll	in5, 8, in5
	or	in5, local3, in5

	ldub	[local5+1], local3
	sll	in5, 8, in5
	or	in5, local3, in5

	ldub	[local5+0], local3
	sll	in5, 8, in5
	or	in5, local3, in5


	ldub	[local5+3+4], out5

	ldub	[local5+2+4], local3
	sll	out5, 8, out5
	or	out5, local3, out5

	ldub	[local5+1+4], local3
	sll	out5, 8, out5
	or	out5, local3, out5

	ldub	[local5+0+4], local3
	sll	out5, 8, out5
	or	out5, local3, out5
.LLE9a:



	! parameter 6  1/2 for include encryption/decryption
	! parameter 7  1 for mov in1 to in3
	! parameter 8  1 for mov in3 to in4
	! parameter 9  1 for load ks3 and ks2 to in4 and in3

	

! ip_macro
! in5 out5 in5 out5 in4 2 0 0 1

	ld	[out2+256], local1
	srl	out5, 4, local4

	xor	local4, in5, local4
	nop

	ld	[out2+260], local2
	and	local4, local1, local4
	
	

	ld	[out2+280], out4          ! loop counter
	sll	local4, 4, local1
	xor	in5, local4, in5

	ld	[out2+264], local3
	srl	in5, 16, local4
	xor	out5, local1, out5

	LDPTR	 [%sp+BIAS+ARG0+5*ARGSZ] , in4
	xor	local4, out5, local4
	nop	!sethi	%hi(DES_SPtrans), global1 ! sbox addr

	LDPTR	 [%sp+BIAS+ARG0+4*ARGSZ] , in3
	and	local4, local2, local4
	nop	!or	global1, %lo(DES_SPtrans), global1   ! sbox addr

	sll	local4, 16, local1
	xor	out5, local4, out5

	srl	out5, 2, local4
	xor	in5, local1, in5

	sethi	%hi(16711680), local5
	xor	local4, in5, local4

	and	local4, local3, local4
	or	local5, 255, local5

	sll	local4, 2, local2
	xor	in5, local4, in5

	srl	in5, 8, local4
	xor	out5, local2, out5

	xor	local4, out5, local4
	add	global1, 768, global4

	and	local4, local5, local4
	add	global1, 1024, global5

	ld	[out2+272], local7
	sll	local4, 8, local1
	xor	out5, local4, out5

	srl	out5, 1, local4
	xor	in5, local1, in5

	ld	[in4], out0                ! key 7531
	xor	local4, in5, local4
	add	global1, 256, global2

	ld	[in4+4], out1              ! key 8642
	and	local4, local7, local4
	add	global1, 512, global3

	sll	local4, 1, local1
	xor	in5, local4, in5

	sll	in5, 3, local3
	xor	out5, local1, out5

	sll	out5, 3, local2
	add	global1, 1280, local6     ! address sbox 8

	srl	in5, 29, local4
	add	global1, 1792, out3       ! address sbox 8

	srl	out5, 29, local1
	or	local4, local3, in5

	or	local2, local1, out5

	

	

		ld	[out2+284], local5     ! 0x0000FC00 used in the rounds
		or	local2, local1, out5
		xor	in5, out0, local1

		call .des_dec.1
		and	local1, 252, local1

	
 ! inc .des_dec ks3 in4

	call .des_enc                     ! ks2 in3
	LDPTR	 [%sp+BIAS+ARG0+3*ARGSZ] , in4

	call .des_dec                     ! ks1 in4
	nop

	

! fp_macro
! out5 in5 0 1     

	! initially undo the rotate 3 left done after initial permutation
	! original left is received shifted 3 right and 29 left in local3/4

	sll	in5, 29, local1
	or	local3, local4, out5

	srl	in5, 3, in5
	sethi	%hi(0x55555555), local2

	or	in5, local1, in5
	or	local2, %lo(0x55555555), local2

	srl	in5, 1, local3
	sethi	%hi(0x00ff00ff), local1
	xor	local3, out5, local3
	or	local1, %lo(0x00ff00ff), local1
	and	local3, local2, local3
	sethi	%hi(0x33333333), local4
	sll	local3, 1, local2

	xor	out5, local3, out5

	srl	out5, 8, local3
	xor	in5, local2, in5
	xor	local3, in5, local3
	or	local4, %lo(0x33333333), local4
	and	local3, local1, local3
	sethi	%hi(0x0000ffff), local1
	sll	local3, 8, local2

	xor	in5, local3, in5

	srl	in5, 2, local3
	xor	out5, local2, out5
	xor	local3, out5, local3
	or	local1, %lo(0x0000ffff), local1
	and	local3, local4, local3
	sethi	%hi(0x0f0f0f0f), local4
	sll	local3, 2, local2

	LDPTR  [%sp+BIAS+ARG0+0*ARGSZ] , local5
	xor	out5, local3, out5

	LDPTR  [%sp+BIAS+ARG0+1*ARGSZ] , local7
	srl	out5, 16, local3
	xor	in5, local2, in5
	xor	local3, in5, local3
	or	local4, %lo(0x0f0f0f0f), local4
	and	local3, local1, local3
	sll	local3, 16, local2

	xor	in5, local3, local1

	srl	local1, 4, local3
	xor	out5, local2, out5
	xor	local3, out5, local3
	and	local3, local4, local3
	sll	local3, 4, local2

	xor	out5, local3, out5

	! optional store:

	

	xor	local1, local2, in5

	

   ! 1 for input and output address local5/7

	! in2 is bytes left to be stored
	! in2 is compared to 8 in the rounds

	xor	out5, in0, out4
#ifdef __sparc_v9__
	bl,pn	%icc, .ede3.dec.seven.or.less
#else
	bl	.ede3.dec.seven.or.less
#endif
	xor	in5, in1, global4

	

! load_little_endian_inc
! local5 in0 in1 local3 .LLE10    

	! first in memory to rightmost in register

#ifdef __sparc_v9__
	andcc	local5, 3, global0
	bne,pn	%icc, .LLE10
	nop

	lda	[local5] 0x88, in0
	add	local5, 4, local5

	lda	[local5] 0x88, in1
	ba,pt	%icc, .LLE10a
	add	local5, 4, local5
#endif

.LLE10:
	ldub	[local5+3], in0

	ldub	[local5+2], local3
	sll	in0, 8, in0
	or	in0, local3, in0

	ldub	[local5+1], local3
	sll	in0, 8, in0
	or	in0, local3, in0

	ldub	[local5+0], local3
	sll	in0, 8, in0
	or	in0, local3, in0

	ldub	[local5+3+4], in1
	add	local5, 8, local5

	ldub	[local5+2+4-8], local3
	sll	in1, 8, in1
	or	in1, local3, in1

	ldub	[local5+1+4-8], local3
	sll	in1, 8, in1
	or	in1, local3, in1

	ldub	[local5+0+4-8], local3
	sll	in1, 8, in1
	or	in1, local3, in1
.LLE10a:

   ! iv next block

	

! store_little_endian
! local7 out4 global4 local3 .SLE7    

	! rightmost in register to first in memory

#ifdef __sparc_v9__
	andcc	local7, 3, global0
	bne,pn	%icc, .SLE7
	nop

	sta	out4, [local7] 0x88
	add	local7, 4, local3

	ba,pt	%icc, .SLE7a
	sta	global4, [local3] 0x88
#endif

.SLE7:
	and	out4, 255, local3
	stub	local3, [local7+0]

	srl	out4, 8, local3
	and	local3, 255, local3
	stub	local3, [local7+1]

	srl	out4, 16, local3
	and	local3, 255, local3
	stub	local3, [local7+2]

	srl	out4, 24, local3
	stub	local3, [local7+3]


	and	global4, 255, local3
	stub	local3, [local7+0+4]

	srl	global4, 8, local3
	and	local3, 255, local3
	stub	local3, [local7+1+4]

	srl	global4, 16, local3
	and	local3, 255, local3
	stub	local3, [local7+2+4]

	srl	global4, 24, local3
	stub	local3, [local7+3+4]

.SLE7a:

  ! block

	STPTR	local5,  [%sp+BIAS+ARG0+0*ARGSZ] 
	addcc   in2, -8, in2
	add	local7, 8, local7

#ifdef __sparc_v9__
	bg,pt	%icc, .ede3.dec.next.block
#else
	bg	.ede3.dec.next.block
#endif
	STPTR	local7,  [%sp+BIAS+ARG0+1*ARGSZ] 

.ede3.dec.store.iv:

	LDPTR	[%fp+BIAS+ARG0+6*ARGSZ], local4          ! ivec
	

! store_little_endian
! local4 in0 in1 local5 .SLE8    

	! rightmost in register to first in memory

#ifdef __sparc_v9__
	andcc	local4, 3, global0
	bne,pn	%icc, .SLE8
	nop

	sta	in0, [local4] 0x88
	add	local4, 4, local5

	ba,pt	%icc, .SLE8a
	sta	in1, [local5] 0x88
#endif

.SLE8:
	and	in0, 255, local5
	stub	local5, [local4+0]

	srl	in0, 8, local5
	and	local5, 255, local5
	stub	local5, [local4+1]

	srl	in0, 16, local5
	and	local5, 255, local5
	stub	local5, [local4+2]

	srl	in0, 24, local5
	stub	local5, [local4+3]


	and	in1, 255, local5
	stub	local5, [local4+0+4]

	srl	in1, 8, local5
	and	local5, 255, local5
	stub	local5, [local4+1+4]

	srl	in1, 16, local5
	and	local5, 255, local5
	stub	local5, [local4+2+4]

	srl	in1, 24, local5
	stub	local5, [local4+3+4]

.SLE8a:

  ! ivec

.ede3.dec.finish:

	ret
	restore

.ede3.dec.seven.or.less:

	

! load_little_endian_inc
! local5 in0 in1 local3 .LLE14    

	! first in memory to rightmost in register

#ifdef __sparc_v9__
	andcc	local5, 3, global0
	bne,pn	%icc, .LLE14
	nop

	lda	[local5] 0x88, in0
	add	local5, 4, local5

	lda	[local5] 0x88, in1
	ba,pt	%icc, .LLE14a
	add	local5, 4, local5
#endif

.LLE14:
	ldub	[local5+3], in0

	ldub	[local5+2], local3
	sll	in0, 8, in0
	or	in0, local3, in0

	ldub	[local5+1], local3
	sll	in0, 8, in0
	or	in0, local3, in0

	ldub	[local5+0], local3
	sll	in0, 8, in0
	or	in0, local3, in0

	ldub	[local5+3+4], in1
	add	local5, 8, local5

	ldub	[local5+2+4-8], local3
	sll	in1, 8, in1
	or	in1, local3, in1

	ldub	[local5+1+4-8], local3
	sll	in1, 8, in1
	or	in1, local3, in1

	ldub	[local5+0+4-8], local3
	sll	in1, 8, in1
	or	in1, local3, in1
.LLE14a:

     ! iv

	

! store_n_bytes
! local7 in2 local3 local4 .SNB2 .ede3.dec.store.iv .SNB2 .ede3.dec.store.iv 

.SNB2.0:	call	.+8
	sll	in2, 2, local4

	add	%o7,.SNB2.jmp.table-.SNB2.0,local3

	add	local3, local4, local3

	ld	[local3], local3

	jmp	%o7+local3
	nop

.SNB2.7:
	srl	global4, 16, local3
	and	local3, 0xff, local3
	stub	local3, [local7+6]
.SNB2.6:
	srl	global4, 8, local3
	and	local3, 0xff, local3
	stub	local3, [local7+5]
.SNB2.5:
	and	global4, 0xff, local3
	stub	local3, [local7+4]
.SNB2.4:
	srl	out4, 24, local3
	stub	local3, [local7+3]
.SNB2.3:
	srl	out4, 16, local3
	and	local3, 0xff, local3
	stub	local3, [local7+2]
.SNB2.2:
	srl	out4, 8, local3
	and	local3, 0xff, local3
	stub	local3, [local7+1]
.SNB2.1:
	and	out4, 0xff, local3


	ba	.ede3.dec.store.iv
	stub	local3, [local7]

	.align 4

.SNB2.jmp.table:

	.word	0
	.word	.SNB2.1-.SNB2.0
	.word	.SNB2.2-.SNB2.0
	.word	.SNB2.3-.SNB2.0
	.word	.SNB2.4-.SNB2.0
	.word	.SNB2.5-.SNB2.0
	.word	.SNB2.6-.SNB2.0
	.word	.SNB2.7-.SNB2.0



.DES_ede3_cbc_encrypt.end:
	.size	 DES_ede3_cbc_encrypt,.DES_ede3_cbc_encrypt.end-DES_ede3_cbc_encrypt

	.align	256
	.type	 .des_and,#object
	.size	 .des_and,284

.des_and:

! This table is used for AND 0xFC when it is known that register
! bits 8-31 are zero. Makes it possible to do three arithmetic
! operations in one cycle.

	.byte  0, 0, 0, 0, 4, 4, 4, 4
	.byte  8, 8, 8, 8, 12, 12, 12, 12
	.byte  16, 16, 16, 16, 20, 20, 20, 20
	.byte  24, 24, 24, 24, 28, 28, 28, 28
	.byte  32, 32, 32, 32, 36, 36, 36, 36
	.byte  40, 40, 40, 40, 44, 44, 44, 44
	.byte  48, 48, 48, 48, 52, 52, 52, 52
	.byte  56, 56, 56, 56, 60, 60, 60, 60
	.byte  64, 64, 64, 64, 68, 68, 68, 68
	.byte  72, 72, 72, 72, 76, 76, 76, 76
	.byte  80, 80, 80, 80, 84, 84, 84, 84
	.byte  88, 88, 88, 88, 92, 92, 92, 92
	.byte  96, 96, 96, 96, 100, 100, 100, 100
	.byte  104, 104, 104, 104, 108, 108, 108, 108
	.byte  112, 112, 112, 112, 116, 116, 116, 116
	.byte  120, 120, 120, 120, 124, 124, 124, 124
	.byte  128, 128, 128, 128, 132, 132, 132, 132
	.byte  136, 136, 136, 136, 140, 140, 140, 140
	.byte  144, 144, 144, 144, 148, 148, 148, 148
	.byte  152, 152, 152, 152, 156, 156, 156, 156
	.byte  160, 160, 160, 160, 164, 164, 164, 164
	.byte  168, 168, 168, 168, 172, 172, 172, 172
	.byte  176, 176, 176, 176, 180, 180, 180, 180
	.byte  184, 184, 184, 184, 188, 188, 188, 188
	.byte  192, 192, 192, 192, 196, 196, 196, 196
	.byte  200, 200, 200, 200, 204, 204, 204, 204
	.byte  208, 208, 208, 208, 212, 212, 212, 212
	.byte  216, 216, 216, 216, 220, 220, 220, 220
	.byte  224, 224, 224, 224, 228, 228, 228, 228
	.byte  232, 232, 232, 232, 236, 236, 236, 236
	.byte  240, 240, 240, 240, 244, 244, 244, 244
	.byte  248, 248, 248, 248, 252, 252, 252, 252

	! 5 numbers for initil/final permutation

	.word   0x0f0f0f0f                ! offset 256
	.word	0x0000ffff                ! 260
	.word	0x33333333                ! 264
	.word	0x00ff00ff                ! 268
	.word	0x55555555                ! 272

	.word	0                         ! 276
	.word	LOOPS                     ! 280
	.word	0x0000FC00                ! 284

	.global	DES_SPtrans
	.type	DES_SPtrans,#object
	.size	DES_SPtrans,2048
.align	64
DES_SPtrans:
_PIC_DES_SPtrans:
	! nibble 0
	.word	0x02080800, 0x00080000, 0x02000002, 0x02080802
	.word	0x02000000, 0x00080802, 0x00080002, 0x02000002
	.word	0x00080802, 0x02080800, 0x02080000, 0x00000802
	.word	0x02000802, 0x02000000, 0x00000000, 0x00080002
	.word	0x00080000, 0x00000002, 0x02000800, 0x00080800
	.word	0x02080802, 0x02080000, 0x00000802, 0x02000800
	.word	0x00000002, 0x00000800, 0x00080800, 0x02080002
	.word	0x00000800, 0x02000802, 0x02080002, 0x00000000
	.word	0x00000000, 0x02080802, 0x02000800, 0x00080002
	.word	0x02080800, 0x00080000, 0x00000802, 0x02000800
	.word	0x02080002, 0x00000800, 0x00080800, 0x02000002
	.word	0x00080802, 0x00000002, 0x02000002, 0x02080000
	.word	0x02080802, 0x00080800, 0x02080000, 0x02000802
	.word	0x02000000, 0x00000802, 0x00080002, 0x00000000
	.word	0x00080000, 0x02000000, 0x02000802, 0x02080800
	.word	0x00000002, 0x02080002, 0x00000800, 0x00080802
	! nibble 1
	.word	0x40108010, 0x00000000, 0x00108000, 0x40100000
	.word	0x40000010, 0x00008010, 0x40008000, 0x00108000
	.word	0x00008000, 0x40100010, 0x00000010, 0x40008000
	.word	0x00100010, 0x40108000, 0x40100000, 0x00000010
	.word	0x00100000, 0x40008010, 0x40100010, 0x00008000
	.word	0x00108010, 0x40000000, 0x00000000, 0x00100010
	.word	0x40008010, 0x00108010, 0x40108000, 0x40000010
	.word	0x40000000, 0x00100000, 0x00008010, 0x40108010
	.word	0x00100010, 0x40108000, 0x40008000, 0x00108010
	.word	0x40108010, 0x00100010, 0x40000010, 0x00000000
	.word	0x40000000, 0x00008010, 0x00100000, 0x40100010
	.word	0x00008000, 0x40000000, 0x00108010, 0x40008010
	.word	0x40108000, 0x00008000, 0x00000000, 0x40000010
	.word	0x00000010, 0x40108010, 0x00108000, 0x40100000
	.word	0x40100010, 0x00100000, 0x00008010, 0x40008000
	.word	0x40008010, 0x00000010, 0x40100000, 0x00108000
	! nibble 2
	.word	0x04000001, 0x04040100, 0x00000100, 0x04000101
	.word	0x00040001, 0x04000000, 0x04000101, 0x00040100
	.word	0x04000100, 0x00040000, 0x04040000, 0x00000001
	.word	0x04040101, 0x00000101, 0x00000001, 0x04040001
	.word	0x00000000, 0x00040001, 0x04040100, 0x00000100
	.word	0x00000101, 0x04040101, 0x00040000, 0x04000001
	.word	0x04040001, 0x04000100, 0x00040101, 0x04040000
	.word	0x00040100, 0x00000000, 0x04000000, 0x00040101
	.word	0x04040100, 0x00000100, 0x00000001, 0x00040000
	.word	0x00000101, 0x00040001, 0x04040000, 0x04000101
	.word	0x00000000, 0x04040100, 0x00040100, 0x04040001
	.word	0x00040001, 0x04000000, 0x04040101, 0x00000001
	.word	0x00040101, 0x04000001, 0x04000000, 0x04040101
	.word	0x00040000, 0x04000100, 0x04000101, 0x00040100
	.word	0x04000100, 0x00000000, 0x04040001, 0x00000101
	.word	0x04000001, 0x00040101, 0x00000100, 0x04040000
	! nibble 3
	.word	0x00401008, 0x10001000, 0x00000008, 0x10401008
	.word	0x00000000, 0x10400000, 0x10001008, 0x00400008
	.word	0x10401000, 0x10000008, 0x10000000, 0x00001008
	.word	0x10000008, 0x00401008, 0x00400000, 0x10000000
	.word	0x10400008, 0x00401000, 0x00001000, 0x00000008
	.word	0x00401000, 0x10001008, 0x10400000, 0x00001000
	.word	0x00001008, 0x00000000, 0x00400008, 0x10401000
	.word	0x10001000, 0x10400008, 0x10401008, 0x00400000
	.word	0x10400008, 0x00001008, 0x00400000, 0x10000008
	.word	0x00401000, 0x10001000, 0x00000008, 0x10400000
	.word	0x10001008, 0x00000000, 0x00001000, 0x00400008
	.word	0x00000000, 0x10400008, 0x10401000, 0x00001000
	.word	0x10000000, 0x10401008, 0x00401008, 0x00400000
	.word	0x10401008, 0x00000008, 0x10001000, 0x00401008
	.word	0x00400008, 0x00401000, 0x10400000, 0x10001008
	.word	0x00001008, 0x10000000, 0x10000008, 0x10401000
	! nibble 4
	.word	0x08000000, 0x00010000, 0x00000400, 0x08010420
	.word	0x08010020, 0x08000400, 0x00010420, 0x08010000
	.word	0x00010000, 0x00000020, 0x08000020, 0x00010400
	.word	0x08000420, 0x08010020, 0x08010400, 0x00000000
	.word	0x00010400, 0x08000000, 0x00010020, 0x00000420
	.word	0x08000400, 0x00010420, 0x00000000, 0x08000020
	.word	0x00000020, 0x08000420, 0x08010420, 0x00010020
	.word	0x08010000, 0x00000400, 0x00000420, 0x08010400
	.word	0x08010400, 0x08000420, 0x00010020, 0x08010000
	.word	0x00010000, 0x00000020, 0x08000020, 0x08000400
	.word	0x08000000, 0x00010400, 0x08010420, 0x00000000
	.word	0x00010420, 0x08000000, 0x00000400, 0x00010020
	.word	0x08000420, 0x00000400, 0x00000000, 0x08010420
	.word	0x08010020, 0x08010400, 0x00000420, 0x00010000
	.word	0x00010400, 0x08010020, 0x08000400, 0x00000420
	.word	0x00000020, 0x00010420, 0x08010000, 0x08000020
	! nibble 5
	.word	0x80000040, 0x00200040, 0x00000000, 0x80202000
	.word	0x00200040, 0x00002000, 0x80002040, 0x00200000
	.word	0x00002040, 0x80202040, 0x00202000, 0x80000000
	.word	0x80002000, 0x80000040, 0x80200000, 0x00202040
	.word	0x00200000, 0x80002040, 0x80200040, 0x00000000
	.word	0x00002000, 0x00000040, 0x80202000, 0x80200040
	.word	0x80202040, 0x80200000, 0x80000000, 0x00002040
	.word	0x00000040, 0x00202000, 0x00202040, 0x80002000
	.word	0x00002040, 0x80000000, 0x80002000, 0x00202040
	.word	0x80202000, 0x00200040, 0x00000000, 0x80002000
	.word	0x80000000, 0x00002000, 0x80200040, 0x00200000
	.word	0x00200040, 0x80202040, 0x00202000, 0x00000040
	.word	0x80202040, 0x00202000, 0x00200000, 0x80002040
	.word	0x80000040, 0x80200000, 0x00202040, 0x00000000
	.word	0x00002000, 0x80000040, 0x80002040, 0x80202000
	.word	0x80200000, 0x00002040, 0x00000040, 0x80200040
	! nibble 6
	.word	0x00004000, 0x00000200, 0x01000200, 0x01000004
	.word	0x01004204, 0x00004004, 0x00004200, 0x00000000
	.word	0x01000000, 0x01000204, 0x00000204, 0x01004000
	.word	0x00000004, 0x01004200, 0x01004000, 0x00000204
	.word	0x01000204, 0x00004000, 0x00004004, 0x01004204
	.word	0x00000000, 0x01000200, 0x01000004, 0x00004200
	.word	0x01004004, 0x00004204, 0x01004200, 0x00000004
	.word	0x00004204, 0x01004004, 0x00000200, 0x01000000
	.word	0x00004204, 0x01004000, 0x01004004, 0x00000204
	.word	0x00004000, 0x00000200, 0x01000000, 0x01004004
	.word	0x01000204, 0x00004204, 0x00004200, 0x00000000
	.word	0x00000200, 0x01000004, 0x00000004, 0x01000200
	.word	0x00000000, 0x01000204, 0x01000200, 0x00004200
	.word	0x00000204, 0x00004000, 0x01004204, 0x01000000
	.word	0x01004200, 0x00000004, 0x00004004, 0x01004204
	.word	0x01000004, 0x01004200, 0x01004000, 0x00004004
	! nibble 7
	.word	0x20800080, 0x20820000, 0x00020080, 0x00000000
	.word	0x20020000, 0x00800080, 0x20800000, 0x20820080
	.word	0x00000080, 0x20000000, 0x00820000, 0x00020080
	.word	0x00820080, 0x20020080, 0x20000080, 0x20800000
	.word	0x00020000, 0x00820080, 0x00800080, 0x20020000
	.word	0x20820080, 0x20000080, 0x00000000, 0x00820000
	.word	0x20000000, 0x00800000, 0x20020080, 0x20800080
	.word	0x00800000, 0x00020000, 0x20820000, 0x00000080
	.word	0x00800000, 0x00020000, 0x20000080, 0x20820080
	.word	0x00020080, 0x20000000, 0x00000000, 0x00820000
	.word	0x20800080, 0x20020080, 0x20020000, 0x00800080
	.word	0x20820000, 0x00000080, 0x00800080, 0x20020000
	.word	0x20820080, 0x00800000, 0x20800000, 0x20000080
	.word	0x00820000, 0x00020080, 0x20020080, 0x20800000
	.word	0x00000080, 0x20820000, 0x00820080, 0x00000000
	.word	0x20000000, 0x20800080, 0x00020000, 0x00820080

