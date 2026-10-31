/*	$NetBSD: crt0-efi-ia64.S,v 1.1.1.1 2014/04/01 16:16:08 jakllsch Exp $	*/

/* crt0-efi-ia64.S - IA-64 EFI startup code.
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
	.psr abi64
	.psr lsb
	.lsb

	.proc _start
_start:
	alloc loc0=ar.pfs,2,2,2,0
	mov loc1=rp
	movl out0=@gprel(ImageBase)	// out0 <- ImageBase (ldbase)
	;;
	add out0=out0,gp
	movl out1=@gprel(_DYNAMIC)	// out1 <- _DYNAMIC
	;;		// avoid WAW on CFM
	add out1=out1,gp
	br.call.sptk.few rp=_relocate
.Lret0:	
	cmp.ne p6,p0=r0,r8		// r8 == EFI_SUCCESS?
(p6)	br.cond.sptk.few .exit		// no ->

.Lret1:

	mov out0=in0			// image handle
	mov out1=in1			// systab
	br.call.sptk.few rp=efi_main
.Lret2:
.exit:
	mov ar.pfs=loc0
	mov rp=loc1
	;;
	br.ret.sptk.few rp

	.endp _start


	// PE32+ wants a PLABEL, not the code address of the entry point:

	.align 16
	.global _start_plabel
	.section .plabel, "a"
_start_plabel:
	data8	_start
	data8	__gp

	// hand-craft a .reloc section for the plabel:

#define IMAGE_REL_BASED_DIR64	10

	.section .reloc, "a"
	data4	_start_plabel				// Page RVA
	data4	12					// Block Size (2*4+2*2)
	data2	(IMAGE_REL_BASED_DIR64<<12) +  0	// reloc for plabel's entry point
	data2	(IMAGE_REL_BASED_DIR64<<12) +  8	// reloc for plabel's global pointer
