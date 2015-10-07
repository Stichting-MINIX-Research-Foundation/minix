.file	"aesni-x86.s"
.text
.ascii	"AES for Intel AES-NI, CRYPTOGAMS by <appro@openssl.org>\0"
.globl	aesni_encrypt
.type	aesni_encrypt,@function
.align	16
aesni_encrypt:
.L_aesni_encrypt_begin:
	movl	4(%esp),%eax
	movl	12(%esp),%edx
	movups	(%eax),%xmm0
	movl	240(%edx),%ecx
	movl	8(%esp),%eax
	movups	(%edx),%xmm3
	movups	16(%edx),%xmm4
	leal	32(%edx),%edx
	pxor	%xmm3,%xmm0
.L000enc1_loop:
	aesenc	%xmm4,%xmm0
	decl	%ecx
	movups	(%edx),%xmm4
	leal	16(%edx),%edx
	jnz	.L000enc1_loop
	aesenclast	%xmm4,%xmm0
	movups	%xmm0,(%eax)
	ret
.size	aesni_encrypt,.-.L_aesni_encrypt_begin
.globl	aesni_decrypt
.type	aesni_decrypt,@function
.align	16
aesni_decrypt:
.L_aesni_decrypt_begin:
	movl	4(%esp),%eax
	movl	12(%esp),%edx
	movups	(%eax),%xmm0
	movl	240(%edx),%ecx
	movl	8(%esp),%eax
	movups	(%edx),%xmm3
	movups	16(%edx),%xmm4
	leal	32(%edx),%edx
	pxor	%xmm3,%xmm0
.L001dec1_loop:
	aesdec	%xmm4,%xmm0
	decl	%ecx
	movups	(%edx),%xmm4
	leal	16(%edx),%edx
	jnz	.L001dec1_loop
	aesdeclast	%xmm4,%xmm0
	movups	%xmm0,(%eax)
	ret
.size	aesni_decrypt,.-.L_aesni_decrypt_begin
.type	_aesni_encrypt3,@function
.align	16
_aesni_encrypt3:
	movups	(%edx),%xmm3
	shrl	$1,%ecx
	movups	16(%edx),%xmm4
	leal	32(%edx),%edx
	pxor	%xmm3,%xmm0
	pxor	%xmm3,%xmm1
	pxor	%xmm3,%xmm2
	jmp	.L002enc3_loop
.align	16
.L002enc3_loop:
	aesenc	%xmm4,%xmm0
	movups	(%edx),%xmm3
	aesenc	%xmm4,%xmm1
	decl	%ecx
	aesenc	%xmm4,%xmm2
	movups	16(%edx),%xmm4
	aesenc	%xmm3,%xmm0
	leal	32(%edx),%edx
	aesenc	%xmm3,%xmm1
	aesenc	%xmm3,%xmm2
	jnz	.L002enc3_loop
	aesenc	%xmm4,%xmm0
	movups	(%edx),%xmm3
	aesenc	%xmm4,%xmm1
	aesenc	%xmm4,%xmm2
	aesenclast	%xmm3,%xmm0
	aesenclast	%xmm3,%xmm1
	aesenclast	%xmm3,%xmm2
	ret
.size	_aesni_encrypt3,.-_aesni_encrypt3
.type	_aesni_decrypt3,@function
.align	16
_aesni_decrypt3:
	movups	(%edx),%xmm3
	shrl	$1,%ecx
	movups	16(%edx),%xmm4
	leal	32(%edx),%edx
	pxor	%xmm3,%xmm0
	pxor	%xmm3,%xmm1
	pxor	%xmm3,%xmm2
	jmp	.L003dec3_loop
.align	16
.L003dec3_loop:
	aesdec	%xmm4,%xmm0
	movups	(%edx),%xmm3
	aesdec	%xmm4,%xmm1
	decl	%ecx
	aesdec	%xmm4,%xmm2
	movups	16(%edx),%xmm4
	aesdec	%xmm3,%xmm0
	leal	32(%edx),%edx
	aesdec	%xmm3,%xmm1
	aesdec	%xmm3,%xmm2
	jnz	.L003dec3_loop
	aesdec	%xmm4,%xmm0
	movups	(%edx),%xmm3
	aesdec	%xmm4,%xmm1
	aesdec	%xmm4,%xmm2
	aesdeclast	%xmm3,%xmm0
	aesdeclast	%xmm3,%xmm1
	aesdeclast	%xmm3,%xmm2
	ret
.size	_aesni_decrypt3,.-_aesni_decrypt3
.type	_aesni_encrypt4,@function
.align	16
_aesni_encrypt4:
	movups	(%edx),%xmm3
	movups	16(%edx),%xmm4
	shrl	$1,%ecx
	leal	32(%edx),%edx
	pxor	%xmm3,%xmm0
	pxor	%xmm3,%xmm1
	pxor	%xmm3,%xmm2
	pxor	%xmm3,%xmm7
	jmp	.L004enc3_loop
.align	16
.L004enc3_loop:
	aesenc	%xmm4,%xmm0
	movups	(%edx),%xmm3
	aesenc	%xmm4,%xmm1
	decl	%ecx
	aesenc	%xmm4,%xmm2
	aesenc	%xmm4,%xmm7
	movups	16(%edx),%xmm4
	aesenc	%xmm3,%xmm0
	leal	32(%edx),%edx
	aesenc	%xmm3,%xmm1
	aesenc	%xmm3,%xmm2
	aesenc	%xmm3,%xmm7
	jnz	.L004enc3_loop
	aesenc	%xmm4,%xmm0
	movups	(%edx),%xmm3
	aesenc	%xmm4,%xmm1
	aesenc	%xmm4,%xmm2
	aesenc	%xmm4,%xmm7
	aesenclast	%xmm3,%xmm0
	aesenclast	%xmm3,%xmm1
	aesenclast	%xmm3,%xmm2
	aesenclast	%xmm3,%xmm7
	ret
.size	_aesni_encrypt4,.-_aesni_encrypt4
.type	_aesni_decrypt4,@function
.align	16
_aesni_decrypt4:
	movups	(%edx),%xmm3
	movups	16(%edx),%xmm4
	shrl	$1,%ecx
	leal	32(%edx),%edx
	pxor	%xmm3,%xmm0
	pxor	%xmm3,%xmm1
	pxor	%xmm3,%xmm2
	pxor	%xmm3,%xmm7
	jmp	.L005dec3_loop
.align	16
.L005dec3_loop:
	aesdec	%xmm4,%xmm0
	movups	(%edx),%xmm3
	aesdec	%xmm4,%xmm1
	decl	%ecx
	aesdec	%xmm4,%xmm2
	aesdec	%xmm4,%xmm7
	movups	16(%edx),%xmm4
	aesdec	%xmm3,%xmm0
	leal	32(%edx),%edx
	aesdec	%xmm3,%xmm1
	aesdec	%xmm3,%xmm2
	aesdec	%xmm3,%xmm7
	jnz	.L005dec3_loop
	aesdec	%xmm4,%xmm0
	movups	(%edx),%xmm3
	aesdec	%xmm4,%xmm1
	aesdec	%xmm4,%xmm2
	aesdec	%xmm4,%xmm7
	aesdeclast	%xmm3,%xmm0
	aesdeclast	%xmm3,%xmm1
	aesdeclast	%xmm3,%xmm2
	aesdeclast	%xmm3,%xmm7
	ret
.size	_aesni_decrypt4,.-_aesni_decrypt4
.globl	aesni_ecb_encrypt
.type	aesni_ecb_encrypt,@function
.align	16
aesni_ecb_encrypt:
.L_aesni_ecb_encrypt_begin:
	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movl	20(%esp),%esi
	movl	24(%esp),%edi
	movl	28(%esp),%eax
	movl	32(%esp),%edx
	movl	36(%esp),%ecx
	cmpl	$16,%eax
	jb	.L006ecb_ret
	andl	$-16,%eax
	testl	%ecx,%ecx
	movl	240(%edx),%ecx
	movl	%edx,%ebp
	movl	%ecx,%ebx
	jz	.L007ecb_decrypt
	subl	$64,%eax
	jbe	.L008ecb_enc_tail
	jmp	.L009ecb_enc_loop3
.align	16
.L009ecb_enc_loop3:
	movups	(%esi),%xmm0
	movups	16(%esi),%xmm1
	movups	32(%esi),%xmm2
	call	_aesni_encrypt3
	subl	$48,%eax
	leal	48(%esi),%esi
	leal	48(%edi),%edi
	movups	%xmm0,-48(%edi)
	movl	%ebp,%edx
	movups	%xmm1,-32(%edi)
	movl	%ebx,%ecx
	movups	%xmm2,-16(%edi)
	ja	.L009ecb_enc_loop3
.L008ecb_enc_tail:
	addl	$64,%eax
	jz	.L006ecb_ret
	cmpl	$16,%eax
	movups	(%esi),%xmm0
	je	.L010ecb_enc_one
	cmpl	$32,%eax
	movups	16(%esi),%xmm1
	je	.L011ecb_enc_two
	cmpl	$48,%eax
	movups	32(%esi),%xmm2
	je	.L012ecb_enc_three
	movups	48(%esi),%xmm7
	call	_aesni_encrypt4
	movups	%xmm0,(%edi)
	movups	%xmm1,16(%edi)
	movups	%xmm2,32(%edi)
	movups	%xmm7,48(%edi)
	jmp	.L006ecb_ret
.align	16
.L010ecb_enc_one:
	movups	(%edx),%xmm3
	movups	16(%edx),%xmm4
	leal	32(%edx),%edx
	pxor	%xmm3,%xmm0
.L013enc1_loop:
	aesenc	%xmm4,%xmm0
	decl	%ecx
	movups	(%edx),%xmm4
	leal	16(%edx),%edx
	jnz	.L013enc1_loop
	aesenclast	%xmm4,%xmm0
	movups	%xmm0,(%edi)
	jmp	.L006ecb_ret
.align	16
.L011ecb_enc_two:
	call	_aesni_encrypt3
	movups	%xmm0,(%edi)
	movups	%xmm1,16(%edi)
	jmp	.L006ecb_ret
.align	16
.L012ecb_enc_three:
	call	_aesni_encrypt3
	movups	%xmm0,(%edi)
	movups	%xmm1,16(%edi)
	movups	%xmm2,32(%edi)
	jmp	.L006ecb_ret
.align	16
.L007ecb_decrypt:
	subl	$64,%eax
	jbe	.L014ecb_dec_tail
	jmp	.L015ecb_dec_loop3
.align	16
.L015ecb_dec_loop3:
	movups	(%esi),%xmm0
	movups	16(%esi),%xmm1
	movups	32(%esi),%xmm2
	call	_aesni_decrypt3
	subl	$48,%eax
	leal	48(%esi),%esi
	leal	48(%edi),%edi
	movups	%xmm0,-48(%edi)
	movl	%ebp,%edx
	movups	%xmm1,-32(%edi)
	movl	%ebx,%ecx
	movups	%xmm2,-16(%edi)
	ja	.L015ecb_dec_loop3
.L014ecb_dec_tail:
	addl	$64,%eax
	jz	.L006ecb_ret
	cmpl	$16,%eax
	movups	(%esi),%xmm0
	je	.L016ecb_dec_one
	cmpl	$32,%eax
	movups	16(%esi),%xmm1
	je	.L017ecb_dec_two
	cmpl	$48,%eax
	movups	32(%esi),%xmm2
	je	.L018ecb_dec_three
	movups	48(%esi),%xmm7
	call	_aesni_decrypt4
	movups	%xmm0,(%edi)
	movups	%xmm1,16(%edi)
	movups	%xmm2,32(%edi)
	movups	%xmm7,48(%edi)
	jmp	.L006ecb_ret
.align	16
.L016ecb_dec_one:
	movups	(%edx),%xmm3
	movups	16(%edx),%xmm4
	leal	32(%edx),%edx
	pxor	%xmm3,%xmm0
.L019dec1_loop:
	aesdec	%xmm4,%xmm0
	decl	%ecx
	movups	(%edx),%xmm4
	leal	16(%edx),%edx
	jnz	.L019dec1_loop
	aesdeclast	%xmm4,%xmm0
	movups	%xmm0,(%edi)
	jmp	.L006ecb_ret
.align	16
.L017ecb_dec_two:
	call	_aesni_decrypt3
	movups	%xmm0,(%edi)
	movups	%xmm1,16(%edi)
	jmp	.L006ecb_ret
.align	16
.L018ecb_dec_three:
	call	_aesni_decrypt3
	movups	%xmm0,(%edi)
	movups	%xmm1,16(%edi)
	movups	%xmm2,32(%edi)
.L006ecb_ret:
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
.size	aesni_ecb_encrypt,.-.L_aesni_ecb_encrypt_begin
.globl	aesni_cbc_encrypt
.type	aesni_cbc_encrypt,@function
.align	16
aesni_cbc_encrypt:
.L_aesni_cbc_encrypt_begin:
	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movl	20(%esp),%esi
	movl	24(%esp),%edi
	movl	28(%esp),%eax
	movl	32(%esp),%edx
	testl	%eax,%eax
	movl	36(%esp),%ebp
	jz	.L020cbc_ret
	cmpl	$0,40(%esp)
	movups	(%ebp),%xmm5
	movl	240(%edx),%ecx
	movl	%edx,%ebp
	movl	%ecx,%ebx
	je	.L021cbc_decrypt
	movaps	%xmm5,%xmm0
	cmpl	$16,%eax
	jb	.L022cbc_enc_tail
	subl	$16,%eax
	jmp	.L023cbc_enc_loop
.align	16
.L023cbc_enc_loop:
	movups	(%esi),%xmm5
	leal	16(%esi),%esi
	pxor	%xmm5,%xmm0
	movups	(%edx),%xmm3
	movups	16(%edx),%xmm4
	leal	32(%edx),%edx
	pxor	%xmm3,%xmm0
.L024enc1_loop:
	aesenc	%xmm4,%xmm0
	decl	%ecx
	movups	(%edx),%xmm4
	leal	16(%edx),%edx
	jnz	.L024enc1_loop
	aesenclast	%xmm4,%xmm0
	subl	$16,%eax
	leal	16(%edi),%edi
	movl	%ebx,%ecx
	movl	%ebp,%edx
	movups	%xmm0,-16(%edi)
	jnc	.L023cbc_enc_loop
	addl	$16,%eax
	jnz	.L022cbc_enc_tail
	movaps	%xmm0,%xmm5
	jmp	.L020cbc_ret
.L022cbc_enc_tail:
	movl	%eax,%ecx
.long	2767451785
	movl	$16,%ecx
	subl	%eax,%ecx
	xorl	%eax,%eax
.long	2868115081
	leal	-16(%edi),%edi
	movl	%ebx,%ecx
	movl	%edi,%esi
	movl	%ebp,%edx
	jmp	.L023cbc_enc_loop
.align	16
.L021cbc_decrypt:
	subl	$64,%eax
	jbe	.L025cbc_dec_tail
	jmp	.L026cbc_dec_loop3
.align	16
.L026cbc_dec_loop3:
	movups	(%esi),%xmm0
	movups	16(%esi),%xmm1
	movups	32(%esi),%xmm2
	movaps	%xmm0,%xmm6
	movaps	%xmm1,%xmm7
	call	_aesni_decrypt3
	subl	$48,%eax
	leal	48(%esi),%esi
	leal	48(%edi),%edi
	pxor	%xmm5,%xmm0
	pxor	%xmm6,%xmm1
	movups	-16(%esi),%xmm5
	pxor	%xmm7,%xmm2
	movups	%xmm0,-48(%edi)
	movl	%ebx,%ecx
	movups	%xmm1,-32(%edi)
	movl	%ebp,%edx
	movups	%xmm2,-16(%edi)
	ja	.L026cbc_dec_loop3
.L025cbc_dec_tail:
	addl	$64,%eax
	jz	.L020cbc_ret
	movups	(%esi),%xmm0
	cmpl	$16,%eax
	movaps	%xmm0,%xmm6
	jbe	.L027cbc_dec_one
	movups	16(%esi),%xmm1
	cmpl	$32,%eax
	movaps	%xmm1,%xmm7
	jbe	.L028cbc_dec_two
	movups	32(%esi),%xmm2
	cmpl	$48,%eax
	jbe	.L029cbc_dec_three
	movups	48(%esi),%xmm7
	call	_aesni_decrypt4
	movups	16(%esi),%xmm3
	movups	32(%esi),%xmm4
	pxor	%xmm5,%xmm0
	pxor	%xmm6,%xmm1
	movups	48(%esi),%xmm5
	movups	%xmm0,(%edi)
	pxor	%xmm3,%xmm2
	pxor	%xmm4,%xmm7
	movups	%xmm1,16(%edi)
	movups	%xmm2,32(%edi)
	movaps	%xmm7,%xmm0
	leal	48(%edi),%edi
	jmp	.L030cbc_dec_tail_collected
.L027cbc_dec_one:
	movups	(%edx),%xmm3
	movups	16(%edx),%xmm4
	leal	32(%edx),%edx
	pxor	%xmm3,%xmm0
.L031dec1_loop:
	aesdec	%xmm4,%xmm0
	decl	%ecx
	movups	(%edx),%xmm4
	leal	16(%edx),%edx
	jnz	.L031dec1_loop
	aesdeclast	%xmm4,%xmm0
	pxor	%xmm5,%xmm0
	movaps	%xmm6,%xmm5
	jmp	.L030cbc_dec_tail_collected
.L028cbc_dec_two:
	call	_aesni_decrypt3
	pxor	%xmm5,%xmm0
	pxor	%xmm6,%xmm1
	movups	%xmm0,(%edi)
	movaps	%xmm1,%xmm0
	movaps	%xmm7,%xmm5
	leal	16(%edi),%edi
	jmp	.L030cbc_dec_tail_collected
.L029cbc_dec_three:
	call	_aesni_decrypt3
	pxor	%xmm5,%xmm0
	pxor	%xmm6,%xmm1
	pxor	%xmm7,%xmm2
	movups	%xmm0,(%edi)
	movups	%xmm1,16(%edi)
	movaps	%xmm2,%xmm0
	movups	32(%esi),%xmm5
	leal	32(%edi),%edi
.L030cbc_dec_tail_collected:
	andl	$15,%eax
	jnz	.L032cbc_dec_tail_partial
	movups	%xmm0,(%edi)
	jmp	.L020cbc_ret
.L032cbc_dec_tail_partial:
	movl	%esp,%ebp
	subl	$16,%esp
	andl	$-16,%esp
	movaps	%xmm0,(%esp)
	movl	%esp,%esi
	movl	%eax,%ecx
.long	2767451785
	movl	%ebp,%esp
.L020cbc_ret:
	movl	36(%esp),%ebp
	movups	%xmm5,(%ebp)
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
.size	aesni_cbc_encrypt,.-.L_aesni_cbc_encrypt_begin
.type	_aesni_set_encrypt_key,@function
.align	16
_aesni_set_encrypt_key:
	testl	%eax,%eax
	jz	.L033bad_pointer
	testl	%edx,%edx
	jz	.L033bad_pointer
	movups	(%eax),%xmm0
	pxor	%xmm4,%xmm4
	leal	16(%edx),%edx
	cmpl	$256,%ecx
	je	.L03414rounds
	cmpl	$192,%ecx
	je	.L03512rounds
	cmpl	$128,%ecx
	jne	.L036bad_keybits
.align	16
.L03710rounds:
	movl	$9,%ecx
	movups	%xmm0,-16(%edx)
	aeskeygenassist	$1,%xmm0,%xmm1
	call	.L038key_128_cold
	aeskeygenassist	$2,%xmm0,%xmm1
	call	.L039key_128
	aeskeygenassist	$4,%xmm0,%xmm1
	call	.L039key_128
	aeskeygenassist	$8,%xmm0,%xmm1
	call	.L039key_128
	aeskeygenassist	$16,%xmm0,%xmm1
	call	.L039key_128
	aeskeygenassist	$32,%xmm0,%xmm1
	call	.L039key_128
	aeskeygenassist	$64,%xmm0,%xmm1
	call	.L039key_128
	aeskeygenassist	$128,%xmm0,%xmm1
	call	.L039key_128
	aeskeygenassist	$27,%xmm0,%xmm1
	call	.L039key_128
	aeskeygenassist	$54,%xmm0,%xmm1
	call	.L039key_128
	movups	%xmm0,(%edx)
	movl	%ecx,80(%edx)
	xorl	%eax,%eax
	ret
.align	16
.L039key_128:
	movups	%xmm0,(%edx)
	leal	16(%edx),%edx
.L038key_128_cold:
	shufps	$16,%xmm0,%xmm4
	pxor	%xmm4,%xmm0
	shufps	$140,%xmm0,%xmm4
	pxor	%xmm4,%xmm0
	pshufd	$255,%xmm1,%xmm1
	pxor	%xmm1,%xmm0
	ret
.align	16
.L03512rounds:
	movq	16(%eax),%xmm2
	movl	$11,%ecx
	movups	%xmm0,-16(%edx)
	aeskeygenassist	$1,%xmm2,%xmm1
	call	.L040key_192a_cold
	aeskeygenassist	$2,%xmm2,%xmm1
	call	.L041key_192b
	aeskeygenassist	$4,%xmm2,%xmm1
	call	.L042key_192a
	aeskeygenassist	$8,%xmm2,%xmm1
	call	.L041key_192b
	aeskeygenassist	$16,%xmm2,%xmm1
	call	.L042key_192a
	aeskeygenassist	$32,%xmm2,%xmm1
	call	.L041key_192b
	aeskeygenassist	$64,%xmm2,%xmm1
	call	.L042key_192a
	aeskeygenassist	$128,%xmm2,%xmm1
	call	.L041key_192b
	movups	%xmm0,(%edx)
	movl	%ecx,48(%edx)
	xorl	%eax,%eax
	ret
.align	16
.L042key_192a:
	movups	%xmm0,(%edx)
	leal	16(%edx),%edx
.align	16
.L040key_192a_cold:
	movaps	%xmm2,%xmm5
.L043key_192b_warm:
	shufps	$16,%xmm0,%xmm4
	movaps	%xmm2,%xmm3
	pxor	%xmm4,%xmm0
	shufps	$140,%xmm0,%xmm4
	pslldq	$4,%xmm3
	pxor	%xmm4,%xmm0
	pshufd	$85,%xmm1,%xmm1
	pxor	%xmm3,%xmm2
	pxor	%xmm1,%xmm0
	pshufd	$255,%xmm0,%xmm3
	pxor	%xmm3,%xmm2
	ret
.align	16
.L041key_192b:
	movaps	%xmm0,%xmm3
	shufps	$68,%xmm0,%xmm5
	movups	%xmm5,(%edx)
	shufps	$78,%xmm2,%xmm3
	movups	%xmm3,16(%edx)
	leal	32(%edx),%edx
	jmp	.L043key_192b_warm
.align	16
.L03414rounds:
	movups	16(%eax),%xmm2
	movl	$13,%ecx
	leal	16(%edx),%edx
	movups	%xmm0,-32(%edx)
	movups	%xmm2,-16(%edx)
	aeskeygenassist	$1,%xmm2,%xmm1
	call	.L044key_256a_cold
	aeskeygenassist	$1,%xmm0,%xmm1
	call	.L045key_256b
	aeskeygenassist	$2,%xmm2,%xmm1
	call	.L046key_256a
	aeskeygenassist	$2,%xmm0,%xmm1
	call	.L045key_256b
	aeskeygenassist	$4,%xmm2,%xmm1
	call	.L046key_256a
	aeskeygenassist	$4,%xmm0,%xmm1
	call	.L045key_256b
	aeskeygenassist	$8,%xmm2,%xmm1
	call	.L046key_256a
	aeskeygenassist	$8,%xmm0,%xmm1
	call	.L045key_256b
	aeskeygenassist	$16,%xmm2,%xmm1
	call	.L046key_256a
	aeskeygenassist	$16,%xmm0,%xmm1
	call	.L045key_256b
	aeskeygenassist	$32,%xmm2,%xmm1
	call	.L046key_256a
	aeskeygenassist	$32,%xmm0,%xmm1
	call	.L045key_256b
	aeskeygenassist	$64,%xmm2,%xmm1
	call	.L046key_256a
	movups	%xmm0,(%edx)
	movl	%ecx,16(%edx)
	xorl	%eax,%eax
	ret
.align	16
.L046key_256a:
	movups	%xmm2,(%edx)
	leal	16(%edx),%edx
.L044key_256a_cold:
	shufps	$16,%xmm0,%xmm4
	pxor	%xmm4,%xmm0
	shufps	$140,%xmm0,%xmm4
	pxor	%xmm4,%xmm0
	pshufd	$255,%xmm1,%xmm1
	pxor	%xmm1,%xmm0
	ret
.align	16
.L045key_256b:
	movups	%xmm0,(%edx)
	leal	16(%edx),%edx
	shufps	$16,%xmm2,%xmm4
	pxor	%xmm4,%xmm2
	shufps	$140,%xmm2,%xmm4
	pxor	%xmm4,%xmm2
	pshufd	$170,%xmm1,%xmm1
	pxor	%xmm1,%xmm2
	ret
.align	4
.L033bad_pointer:
	movl	$-1,%eax
	ret
.align	4
.L036bad_keybits:
	movl	$-2,%eax
	ret
.size	_aesni_set_encrypt_key,.-_aesni_set_encrypt_key
.globl	aesni_set_encrypt_key
.type	aesni_set_encrypt_key,@function
.align	16
aesni_set_encrypt_key:
.L_aesni_set_encrypt_key_begin:
	movl	4(%esp),%eax
	movl	8(%esp),%ecx
	movl	12(%esp),%edx
	call	_aesni_set_encrypt_key
	ret
.size	aesni_set_encrypt_key,.-.L_aesni_set_encrypt_key_begin
.globl	aesni_set_decrypt_key
.type	aesni_set_decrypt_key,@function
.align	16
aesni_set_decrypt_key:
.L_aesni_set_decrypt_key_begin:
	movl	4(%esp),%eax
	movl	8(%esp),%ecx
	movl	12(%esp),%edx
	call	_aesni_set_encrypt_key
	movl	12(%esp),%edx
	shll	$4,%ecx
	testl	%eax,%eax
	jnz	.L047dec_key_ret
	leal	16(%edx,%ecx,1),%eax
	movups	(%edx),%xmm0
	movups	(%eax),%xmm1
	movups	%xmm0,(%eax)
	movups	%xmm1,(%edx)
	leal	16(%edx),%edx
	leal	-16(%eax),%eax
.L048dec_key_inverse:
	movups	(%edx),%xmm0
	movups	(%eax),%xmm1
	aesimc	%xmm0,%xmm0
	aesimc	%xmm1,%xmm1
	leal	16(%edx),%edx
	leal	-16(%eax),%eax
	cmpl	%edx,%eax
	movups	%xmm0,16(%eax)
	movups	%xmm1,-16(%edx)
	ja	.L048dec_key_inverse
	movups	(%edx),%xmm0
	aesimc	%xmm0,%xmm0
	movups	%xmm0,(%edx)
	xorl	%eax,%eax
.L047dec_key_ret:
	ret
.size	aesni_set_decrypt_key,.-.L_aesni_set_decrypt_key_begin
