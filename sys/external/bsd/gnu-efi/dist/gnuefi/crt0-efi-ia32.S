/*	$NetBSD: crt0-efi-ia32.S,v 1.1.1.1 2014/04/01 16:16:08 jakllsch Exp $	*/

/* crt0-efi-ia32.S - x86 EFI startup code.
   Copyright (C) 1999 Hewlett-Packard Co.
	Contributed by David Mosberger <davidm@hpl.hp.com>.

    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials
      provided with the distribution.
    * Neither the name of Hewlett-Packard Co. nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
    CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
    INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
    MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
    BE LIABLE FOR ANYDIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
    OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
    PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
    PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
    TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
    THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.
*/

	.text
	.align 4

	.globl _start
_start:
	pushl %ebp
	movl %esp,%ebp

	pushl 12(%ebp)			# copy "image" argument
	pushl  8(%ebp)			# copy "systab" argument

	call 0f
0:	popl %eax
	movl %eax,%ebx

	addl $ImageBase-0b,%eax		# %eax = ldbase
	addl $_DYNAMIC-0b,%ebx		# %ebx = _DYNAMIC

	pushl %ebx			# pass _DYNAMIC as second argument
	pushl %eax			# pass ldbase as first argument
	call _relocate
	popl %ebx
	popl %ebx
 	testl %eax,%eax
 	jne .exit
  
  	call efi_main			# call app with "image" and "systab" argument

.exit:	leave
  	ret
 
 	// hand-craft a dummy .reloc section so EFI knows it's a relocatable executable:
 
 	.data
dummy:	.long	0

#define IMAGE_REL_ABSOLUTE	0
 	.section .reloc
 	.long	dummy					// Page RVA
 	.long	10					// Block Size (2*4+2)
 	.word	(IMAGE_REL_ABSOLUTE<<12) +  0		// reloc for dummy
