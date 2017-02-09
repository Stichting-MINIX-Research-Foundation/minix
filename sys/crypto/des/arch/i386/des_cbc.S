/* $NetBSD: des_cbc.S,v 1.6 2007/12/11 23:31:07 lukem Exp $ */

/* Copyright (C) 1995-1997 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

/*
 * Modified from the output of `perl des686.pl elf' by
 * Thor Lancelot Simon <tls@NetBSD.org>
 */

#include <i386/include/asm.h>
__KERNEL_RCSID(0, "$NetBSD: des_cbc.S,v 1.6 2007/12/11 23:31:07 lukem Exp $");

ENTRY(des_ncbc_encrypt)

	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movl	28(%esp),	%ebp
	# getting iv ptr from parameter 4
	movl	36(%esp),	%ebx
	movl	(%ebx),		%esi
	movl	4(%ebx),	%edi
	pushl	%edi
	pushl	%esi
	pushl	%edi
	pushl	%esi
	movl	%esp,		%ebx
	movl	36(%esp),	%esi
	movl	40(%esp),	%edi
	# getting encrypt flag from parameter 5
	movl	56(%esp),	%ecx
	# get and push parameter 5
	pushl	%ecx
	# get and push parameter 3
	movl	52(%esp),	%eax
	pushl	%eax
	pushl	%ebx
	cmpl	$0,		%ecx
	jz	.L004decrypt
	andl	$4294967288,	%ebp
	movl	12(%esp),	%eax
	movl	16(%esp),	%ebx
	jz	.L005encrypt_finish
.L006encrypt_loop:
	movl	(%esi),		%ecx
	movl	4(%esi),	%edx
	xorl	%ecx,		%eax
	xorl	%edx,		%ebx
	movl	%eax,		12(%esp)
	movl	%ebx,		16(%esp)
	call	_C_LABEL(des_encrypt1)
	movl	12(%esp),	%eax
	movl	16(%esp),	%ebx
	movl	%eax,		(%edi)
	movl	%ebx,		4(%edi)
	addl	$8,		%esi
	addl	$8,		%edi
	subl	$8,		%ebp
	jnz	.L006encrypt_loop
.L005encrypt_finish:
	movl	56(%esp),	%ebp
	andl	$7,		%ebp
	jz	.L007finish
	xorl	%ecx,		%ecx
	xorl	%edx,		%edx
	movl	.L008cbc_enc_jmp_table(,%ebp,4),%ebp
	jmp	*%ebp
.L009ej7:
	movb	6(%esi),	%dh
	sall	$8,		%edx
.L010ej6:
	movb	5(%esi),	%dh
.L011ej5:
	movb	4(%esi),	%dl
.L012ej4:
	movl	(%esi),		%ecx
	jmp	.L013ejend
.L014ej3:
	movb	2(%esi),	%ch
	sall	$8,		%ecx
.L015ej2:
	movb	1(%esi),	%ch
.L016ej1:
	movb	(%esi),		%cl
.L013ejend:
	xorl	%ecx,		%eax
	xorl	%edx,		%ebx
	movl	%eax,		12(%esp)
	movl	%ebx,		16(%esp)
	call	_C_LABEL(des_encrypt1)
	movl	12(%esp),	%eax
	movl	16(%esp),	%ebx
	movl	%eax,		(%edi)
	movl	%ebx,		4(%edi)
	jmp	.L007finish
#ifdef __ELF__
.align 16
#else
.align 4
#endif
.L004decrypt:
	andl	$4294967288,	%ebp
	movl	20(%esp),	%eax
	movl	24(%esp),	%ebx
	jz	.L017decrypt_finish
.L018decrypt_loop:
	movl	(%esi),		%eax
	movl	4(%esi),	%ebx
	movl	%eax,		12(%esp)
	movl	%ebx,		16(%esp)
	call	_C_LABEL(des_encrypt1)
	movl	12(%esp),	%eax
	movl	16(%esp),	%ebx
	movl	20(%esp),	%ecx
	movl	24(%esp),	%edx
	xorl	%eax,		%ecx
	xorl	%ebx,		%edx
	movl	(%esi),		%eax
	movl	4(%esi),	%ebx
	movl	%ecx,		(%edi)
	movl	%edx,		4(%edi)
	movl	%eax,		20(%esp)
	movl	%ebx,		24(%esp)
	addl	$8,		%esi
	addl	$8,		%edi
	subl	$8,		%ebp
	jnz	.L018decrypt_loop
.L017decrypt_finish:
	movl	56(%esp),	%ebp
	andl	$7,		%ebp
	jz	.L007finish
	movl	(%esi),		%eax
	movl	4(%esi),	%ebx
	movl	%eax,		12(%esp)
	movl	%ebx,		16(%esp)
	call	_C_LABEL(des_encrypt1)
	movl	12(%esp),	%eax
	movl	16(%esp),	%ebx
	movl	20(%esp),	%ecx
	movl	24(%esp),	%edx
	xorl	%eax,		%ecx
	xorl	%ebx,		%edx
	movl	(%esi),		%eax
	movl	4(%esi),	%ebx
.L019dj7:
	rorl	$16,		%edx
	movb	%dl,		6(%edi)
	shrl	$16,		%edx
.L020dj6:
	movb	%dh,		5(%edi)
.L021dj5:
	movb	%dl,		4(%edi)
.L022dj4:
	movl	%ecx,		(%edi)
	jmp	.L023djend
.L024dj3:
	rorl	$16,		%ecx
	movb	%cl,		2(%edi)
	sall	$16,		%ecx
.L025dj2:
	movb	%ch,		1(%esi)
.L026dj1:
	movb	%cl,		(%esi)
.L023djend:
	jmp	.L007finish
#ifdef __ELF__
.align 16
#else
.align 4
#endif
.L007finish:
	movl	64(%esp),	%ecx
	addl	$28,		%esp
	movl	%eax,		(%ecx)
	movl	%ebx,		4(%ecx)
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
#ifdef __ELF__
.align 16
#else
.align 4
#endif
.L008cbc_enc_jmp_table:
	.long 0
	.long .L016ej1
	.long .L015ej2
	.long .L014ej3
	.long .L012ej4
	.long .L011ej5
	.long .L010ej6
	.long .L009ej7
#ifdef __ELF__
.align 16
#else
.align 4
#endif
.L027cbc_dec_jmp_table:
	.long 0
	.long .L026dj1
	.long .L025dj2
	.long .L024dj3
	.long .L022dj4
	.long .L021dj5
	.long .L020dj6
	.long .L019dj7
.L_des_ncbc_encrypt_end:
	.size	_C_LABEL(des_ncbc_encrypt),.L_des_ncbc_encrypt_end-_C_LABEL(des_ncbc_encrypt)

ENTRY(des_ede3_cbc_encrypt)

	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movl	28(%esp),	%ebp
	# getting iv ptr from parameter 6
	movl	44(%esp),	%ebx
	movl	(%ebx),		%esi
	movl	4(%ebx),	%edi
	pushl	%edi
	pushl	%esi
	pushl	%edi
	pushl	%esi
	movl	%esp,		%ebx
	movl	36(%esp),	%esi
	movl	40(%esp),	%edi
	# getting encrypt flag from parameter 7
	movl	64(%esp),	%ecx
	# get and push parameter 5
	movl	56(%esp),	%eax
	pushl	%eax
	# get and push parameter 4
	movl	56(%esp),	%eax
	pushl	%eax
	# get and push parameter 3
	movl	56(%esp),	%eax
	pushl	%eax
	pushl	%ebx
	cmpl	$0,		%ecx
	jz	.L028decrypt
	andl	$4294967288,	%ebp
	movl	16(%esp),	%eax
	movl	20(%esp),	%ebx
	jz	.L029encrypt_finish
.L030encrypt_loop:
	movl	(%esi),		%ecx
	movl	4(%esi),	%edx
	xorl	%ecx,		%eax
	xorl	%edx,		%ebx
	movl	%eax,		16(%esp)
	movl	%ebx,		20(%esp)
	call	_C_LABEL(des_encrypt3)
	movl	16(%esp),	%eax
	movl	20(%esp),	%ebx
	movl	%eax,		(%edi)
	movl	%ebx,		4(%edi)
	addl	$8,		%esi
	addl	$8,		%edi
	subl	$8,		%ebp
	jnz	.L030encrypt_loop
.L029encrypt_finish:
	movl	60(%esp),	%ebp
	andl	$7,		%ebp
	jz	.L031finish
	xorl	%ecx,		%ecx
	xorl	%edx,		%edx
	movl	.L032cbc_enc_jmp_table(,%ebp,4),%ebp
	jmp	*%ebp
.L033ej7:
	movb	6(%esi),	%dh
	sall	$8,		%edx
.L034ej6:
	movb	5(%esi),	%dh
.L035ej5:
	movb	4(%esi),	%dl
.L036ej4:
	movl	(%esi),		%ecx
	jmp	.L037ejend
.L038ej3:
	movb	2(%esi),	%ch
	sall	$8,		%ecx
.L039ej2:
	movb	1(%esi),	%ch
.L040ej1:
	movb	(%esi),		%cl
.L037ejend:
	xorl	%ecx,		%eax
	xorl	%edx,		%ebx
	movl	%eax,		16(%esp)
	movl	%ebx,		20(%esp)
	call	_C_LABEL(des_encrypt3)
	movl	16(%esp),	%eax
	movl	20(%esp),	%ebx
	movl	%eax,		(%edi)
	movl	%ebx,		4(%edi)
	jmp	.L031finish
#ifdef __ELF__
.align 16
#else
.align 4
#endif
.L028decrypt:
	andl	$4294967288,	%ebp
	movl	24(%esp),	%eax
	movl	28(%esp),	%ebx
	jz	.L041decrypt_finish
.L042decrypt_loop:
	movl	(%esi),		%eax
	movl	4(%esi),	%ebx
	movl	%eax,		16(%esp)
	movl	%ebx,		20(%esp)
	call	_C_LABEL(des_decrypt3)
	movl	16(%esp),	%eax
	movl	20(%esp),	%ebx
	movl	24(%esp),	%ecx
	movl	28(%esp),	%edx
	xorl	%eax,		%ecx
	xorl	%ebx,		%edx
	movl	(%esi),		%eax
	movl	4(%esi),	%ebx
	movl	%ecx,		(%edi)
	movl	%edx,		4(%edi)
	movl	%eax,		24(%esp)
	movl	%ebx,		28(%esp)
	addl	$8,		%esi
	addl	$8,		%edi
	subl	$8,		%ebp
	jnz	.L042decrypt_loop
.L041decrypt_finish:
	movl	60(%esp),	%ebp
	andl	$7,		%ebp
	jz	.L031finish
	movl	(%esi),		%eax
	movl	4(%esi),	%ebx
	movl	%eax,		16(%esp)
	movl	%ebx,		20(%esp)
	call	_C_LABEL(des_decrypt3)
	movl	16(%esp),	%eax
	movl	20(%esp),	%ebx
	movl	24(%esp),	%ecx
	movl	28(%esp),	%edx
	xorl	%eax,		%ecx
	xorl	%ebx,		%edx
	movl	(%esi),		%eax
	movl	4(%esi),	%ebx
.L043dj7:
	rorl	$16,		%edx
	movb	%dl,		6(%edi)
	shrl	$16,		%edx
.L044dj6:
	movb	%dh,		5(%edi)
.L045dj5:
	movb	%dl,		4(%edi)
.L046dj4:
	movl	%ecx,		(%edi)
	jmp	.L047djend
.L048dj3:
	rorl	$16,		%ecx
	movb	%cl,		2(%edi)
	sall	$16,		%ecx
.L049dj2:
	movb	%ch,		1(%esi)
.L050dj1:
	movb	%cl,		(%esi)
.L047djend:
	jmp	.L031finish
#ifdef __ELF__
.align 16
#else
.align 4
#endif
.L031finish:
	movl	76(%esp),	%ecx
	addl	$32,		%esp
	movl	%eax,		(%ecx)
	movl	%ebx,		4(%ecx)
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
#ifdef __ELF__
.align 16
#else
.align 4
#endif
.L032cbc_enc_jmp_table:
	.long 0
	.long .L040ej1
	.long .L039ej2
	.long .L038ej3
	.long .L036ej4
	.long .L035ej5
	.long .L034ej6
	.long .L033ej7
#ifdef __ELF__
.align 16
#else
.align 4
#endif
.L051cbc_dec_jmp_table:
	.long 0
	.long .L050dj1
	.long .L049dj2
	.long .L048dj3
	.long .L046dj4
	.long .L045dj5
	.long .L044dj6
	.long .L043dj7
.L_des_ede3_cbc_encrypt_end:
	.size	_C_LABEL(des_ede3_cbc_encrypt),.L_des_ede3_cbc_encrypt_end-_C_LABEL(des_ede3_cbc_encrypt)
