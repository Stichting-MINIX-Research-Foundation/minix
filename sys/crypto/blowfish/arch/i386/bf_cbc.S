/* $NetBSD: bf_cbc.S,v 1.6 2007/12/11 23:13:57 lukem Exp $ */

/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
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
 * Modified from the output of `perl bf-686.pl elf' by
 * Thor Lancelot Simon <tls@NetBSD.org>
 */

#include <i386/include/asm.h>
__KERNEL_RCSID(0, "$NetBSD: bf_cbc.S,v 1.6 2007/12/11 23:13:57 lukem Exp $");

ENTRY(BF_cbc_encrypt)

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
	# get and push parameter 3
	movl	48(%esp),	%eax
	pushl	%eax
	pushl	%ebx
	cmpl	$0,		%ecx
	jz	.L000decrypt
	andl	$4294967288,	%ebp
	movl	8(%esp),	%eax
	movl	12(%esp),	%ebx
	jz	.L001encrypt_finish
.L002encrypt_loop:
	movl	(%esi),		%ecx
	movl	4(%esi),	%edx
	xorl	%ecx,		%eax
	xorl	%edx,		%ebx
.byte 15
.byte 200		# bswapl  %eax
.byte 15
.byte 203		# bswapl  %ebx
	movl	%eax,		8(%esp)
	movl	%ebx,		12(%esp)
	call	_C_LABEL(BF_encrypt)
	movl	8(%esp),	%eax
	movl	12(%esp),	%ebx
.byte 15
.byte 200		# bswapl  %eax
.byte 15
.byte 203		# bswapl  %ebx
	movl	%eax,		(%edi)
	movl	%ebx,		4(%edi)
	addl	$8,		%esi
	addl	$8,		%edi
	subl	$8,		%ebp
	jnz	.L002encrypt_loop
.L001encrypt_finish:
	movl	52(%esp),	%ebp
	andl	$7,		%ebp
	jz	.L003finish
	xorl	%ecx,		%ecx
	xorl	%edx,		%edx
	movl	.L004cbc_enc_jmp_table(,%ebp,4),%ebp
	jmp	*%ebp
.L005ej7:
	movb	6(%esi),	%dh
	sall	$8,		%edx
.L006ej6:
	movb	5(%esi),	%dh
.L007ej5:
	movb	4(%esi),	%dl
.L008ej4:
	movl	(%esi),		%ecx
	jmp	.L009ejend
.L010ej3:
	movb	2(%esi),	%ch
	sall	$8,		%ecx
.L011ej2:
	movb	1(%esi),	%ch
.L012ej1:
	movb	(%esi),		%cl
.L009ejend:
	xorl	%ecx,		%eax
	xorl	%edx,		%ebx
.byte 15
.byte 200		# bswapl  %eax
.byte 15
.byte 203		# bswapl  %ebx
	movl	%eax,		8(%esp)
	movl	%ebx,		12(%esp)
	call	_C_LABEL(BF_encrypt)
	movl	8(%esp),	%eax
	movl	12(%esp),	%ebx
.byte 15
.byte 200		# bswapl  %eax
.byte 15
.byte 203		# bswapl  %ebx
	movl	%eax,		(%edi)
	movl	%ebx,		4(%edi)
	jmp	.L003finish
#ifdef __ELF__
.align 16
#else
.align 4
#endif
.L000decrypt:
	andl	$4294967288,	%ebp
	movl	16(%esp),	%eax
	movl	20(%esp),	%ebx
	jz	.L013decrypt_finish
.L014decrypt_loop:
	movl	(%esi),		%eax
	movl	4(%esi),	%ebx
.byte 15
.byte 200		# bswapl  %eax
.byte 15
.byte 203		# bswapl  %ebx
	movl	%eax,		8(%esp)
	movl	%ebx,		12(%esp)
	call	_C_LABEL(BF_decrypt)
	movl	8(%esp),	%eax
	movl	12(%esp),	%ebx
.byte 15
.byte 200		# bswapl  %eax
.byte 15
.byte 203		# bswapl  %ebx
	movl	16(%esp),	%ecx
	movl	20(%esp),	%edx
	xorl	%eax,		%ecx
	xorl	%ebx,		%edx
	movl	(%esi),		%eax
	movl	4(%esi),	%ebx
	movl	%ecx,		(%edi)
	movl	%edx,		4(%edi)
	movl	%eax,		16(%esp)
	movl	%ebx,		20(%esp)
	addl	$8,		%esi
	addl	$8,		%edi
	subl	$8,		%ebp
	jnz	.L014decrypt_loop
.L013decrypt_finish:
	movl	52(%esp),	%ebp
	andl	$7,		%ebp
	jz	.L003finish
	movl	(%esi),		%eax
	movl	4(%esi),	%ebx
.byte 15
.byte 200		# bswapl  %eax
.byte 15
.byte 203		# bswapl  %ebx
	movl	%eax,		8(%esp)
	movl	%ebx,		12(%esp)
	call	_C_LABEL(BF_decrypt)
	movl	8(%esp),	%eax
	movl	12(%esp),	%ebx
.byte 15
.byte 200		# bswapl  %eax
.byte 15
.byte 203		# bswapl  %ebx
	movl	16(%esp),	%ecx
	movl	20(%esp),	%edx
	xorl	%eax,		%ecx
	xorl	%ebx,		%edx
	movl	(%esi),		%eax
	movl	4(%esi),	%ebx
.L015dj7:
	rorl	$16,		%edx
	movb	%dl,		6(%edi)
	shrl	$16,		%edx
.L016dj6:
	movb	%dh,		5(%edi)
.L017dj5:
	movb	%dl,		4(%edi)
.L018dj4:
	movl	%ecx,		(%edi)
	jmp	.L019djend
.L020dj3:
	rorl	$16,		%ecx
	movb	%cl,		2(%edi)
	sall	$16,		%ecx
.L021dj2:
	movb	%ch,		1(%esi)
.L022dj1:
	movb	%cl,		(%esi)
.L019djend:
	jmp	.L003finish
#ifdef __ELF__
.align 16
#else
.align 4
#endif
.L003finish:
	movl	60(%esp),	%ecx
	addl	$24,		%esp
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
.L004cbc_enc_jmp_table:
	.long 0
	.long .L012ej1
	.long .L011ej2
	.long .L010ej3
	.long .L008ej4
	.long .L007ej5
	.long .L006ej6
	.long .L005ej7
#ifdef __ELF__
.align 16
#else
.align 4
#endif
.L023cbc_dec_jmp_table:
	.long 0
	.long .L022dj1
	.long .L021dj2
	.long .L020dj3
	.long .L018dj4
	.long .L017dj5
	.long .L016dj6
	.long .L015dj7
.L_BF_cbc_encrypt_end:
	.size	_C_LABEL(BF_cbc_encrypt),.L_BF_cbc_encrypt_end-_C_LABEL(BF_cbc_encrypt)
