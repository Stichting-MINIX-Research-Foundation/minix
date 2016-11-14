/*	$NetBSD: locore.S,v 1.114 2015/08/30 01:46:03 uebayasi Exp $	*/

/*
 * Copyright-o-rama!
 */

/*
 * Copyright (c) 2006 Manuel Bouyer.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Frank van der Linden for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


/*-
 * Copyright (c) 1998, 2000, 2004, 2006, 2007, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum, and by Andrew Doran.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)locore.s	7.3 (Berkeley) 5/13/91
 */

#include <machine/asm.h>
__KERNEL_RCSID(0, "$NetBSD: locore.S,v 1.114 2015/08/30 01:46:03 uebayasi Exp $");

#include "opt_compat_oldboot.h"
#include "opt_copy_symtab.h"
#include "opt_ddb.h"
#include "opt_modular.h"
#include "opt_multiboot.h"
#include "opt_realmem.h"
#include "opt_vm86.h"
#include "opt_xen.h"

#include "assym.h"
#include "lapic.h"
#include "ioapic.h"
#include "ksyms.h"

#include <sys/errno.h>
#include <sys/syscall.h>

#include <machine/cputypes.h>
#include <machine/segments.h>
#include <machine/specialreg.h>
#include <machine/trap.h>
#include <machine/i82489reg.h>
#include <machine/frameasm.h>
#include <machine/i82489reg.h>
#ifndef XEN
#include <machine/multiboot.h>
#endif

/* XXX temporary kluge; these should not be here */
/* Get definitions for IOM_BEGIN, IOM_END, and IOM_SIZE */
#include <dev/isa/isareg.h>

#ifndef XEN
#define	_RELOC(x)	((x) - KERNBASE)
#else
#define	_RELOC(x)	((x))
#endif /* XEN */
#define	RELOC(x)	_RELOC(_C_LABEL(x))

#ifdef XEN
/*
 * Xen guest identifier and loader selection
 */
.section __xen_guest
	.ascii	"GUEST_OS=netbsd,GUEST_VER=3.0,XEN_VER=xen-3.0"
	.ascii	",VIRT_BASE=0xc0000000" /* KERNBASE */
	.ascii	",ELF_PADDR_OFFSET=0xc0000000" /* KERNBASE */
	.ascii	",VIRT_ENTRY=0xc0100000" /* KERNTEXTOFF */
	.ascii	",HYPERCALL_PAGE=0x00000101"
		/* (???+HYPERCALL_PAGE_OFFSET)/PAGE_SIZE) */
#ifdef PAE
	.ascii	",PAE=yes[extended-cr3]"
#endif
	.ascii	",LOADER=generic"
#if (NKSYMS || defined(DDB) || defined(MODULAR)) && !defined(makeoptions_COPY_SYMTAB)
	.ascii	",BSD_SYMTAB=yes"
#endif
	.byte	0
#endif	/* XEN */

/*
 * Initialization
 */
	.data

	.globl	_C_LABEL(cputype)
	.globl	_C_LABEL(cpuid_level)
	.globl	_C_LABEL(esym)
	.globl	_C_LABEL(eblob)
	.globl	_C_LABEL(atdevbase)
	.globl	_C_LABEL(lwp0uarea)
	.globl	_C_LABEL(PDPpaddr)
	.globl	_C_LABEL(gdt)
	.globl	_C_LABEL(idt)
	.globl	_C_LABEL(lapic_tpr)

#if NLAPIC > 0
#ifdef __ELF__
	.align	PAGE_SIZE
#else
	.align	12
#endif
	.globl _C_LABEL(local_apic), _C_LABEL(lapic_id)
	.type	_C_LABEL(local_apic), @object
LABEL(local_apic)
	.space	LAPIC_ID
END(local_apic)
	.type	_C_LABEL(lapic_id), @object
LABEL(lapic_id)
	.long	0x00000000
	.space	LAPIC_TPRI-(LAPIC_ID+4)
END(lapic_id)
	.type	_C_LABEL(lapic_tpr), @object
LABEL(lapic_tpr)
	.space	LAPIC_PPRI-LAPIC_TPRI
END(lapic_tpr)
	.type	_C_LABEL(lapic_ppr), @object
_C_LABEL(lapic_ppr):
	.space	LAPIC_ISR-LAPIC_PPRI
END(lapic_ppr)
	.type	_C_LABEL(lapic_isr), @object
_C_LABEL(lapic_isr):
	.space	PAGE_SIZE-LAPIC_ISR
END(lapic_isr)
#else
	.type	_C_LABEL(lapic_tpr), @object
LABEL(lapic_tpr)
	.long 0
END(lapic_tpr)
#endif
	.type	_C_LABEL(cputype), @object
LABEL(cputype)		.long	0	# are we 80486, Pentium, or..
END(cputype)
	.type	_C_LABEL(cpuid_level), @object
LABEL(cpuid_level)	.long	0
END(cpuid_level)
	.type	_C_LABEL(atdevbase), @object
LABEL(atdevbase)	.long	0	# location of start of iomem in virtual
END(atdevbase)
	.type	_C_LABEL(lwp0uarea), @object
LABEL(lwp0uarea)	.long	0
END(lwp0uarea)
	.type	_C_LABEL(PDPpaddr), @object
LABEL(PDPpaddr)		.long	0	# paddr of PDP, for libkvm
END(PDPpaddr)
	.type	_C_LABEL(tablesize), @object
_C_LABEL(tablesize):	.long	0
END(tablesize)
	.size	tmpstk, tmpstk - .
	.space 512
tmpstk:
#ifdef XEN
	.align 		PAGE_SIZE, 0x0	# Align on page boundary
LABEL(tmpgdt)
	.space 		PAGE_SIZE	# Xen expects a page
END(tmpgdt)
#endif /* XEN */

	.text
	.globl	_C_LABEL(kernel_text)
	.set	_C_LABEL(kernel_text),KERNTEXTOFF

ENTRY(start)
#ifndef XEN
	movw	$0x1234,0x472			# warm boot
#if defined(MULTIBOOT)
	jmp	1f

	.align	4
	.globl	Multiboot_Header
_C_LABEL(Multiboot_Header):
#define MULTIBOOT_HEADER_FLAGS	(MULTIBOOT_HEADER_WANT_MEMORY)
	.long	MULTIBOOT_HEADER_MAGIC
	.long	MULTIBOOT_HEADER_FLAGS
	.long	-(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)

1:
	/* Check if we are being executed by a Multiboot-compliant boot
	 * loader. */
	cmpl	$MULTIBOOT_INFO_MAGIC,%eax
	jne	1f

	/*
	 * Indeed, a multiboot-compliant boot loader executed us.  We copy
	 * the received Multiboot information structure into kernel's data
	 * space to process it later -- after we are relocated.  It will
	 * be safer to run complex C code than doing it at this point.
	 */
	pushl	%ebx		# Address of Multiboot information
	call	_C_LABEL(multiboot_pre_reloc)
	addl	$4,%esp
	jmp	2f
#endif

1:
	/*
	 * At this point, we know that a NetBSD-specific boot loader
	 * booted this kernel.  The stack carries the following parameters:
	 * (boothowto, [bootdev], bootinfo, esym, biosextmem, biosbasemem),
	 * 4 bytes each.
	 */
	addl	$4,%esp		# Discard return address to boot loader
	call	_C_LABEL(native_loader)
	addl	$24,%esp

2:
	/* First, reset the PSL. */
	pushl	$PSL_MBO
	popfl

	/* Clear segment registers; always null in proc0. */
	xorl	%eax,%eax
	movw	%ax,%fs
	movw	%ax,%gs
	decl	%eax
	movl	%eax,RELOC(cpuid_level)

	/* Find out our CPU type. */

try386:	/* Try to toggle alignment check flag; does not exist on 386. */
	pushfl
	popl	%eax
	movl	%eax,%ecx
	orl	$PSL_AC,%eax
	pushl	%eax
	popfl
	pushfl
	popl	%eax
	xorl	%ecx,%eax
	andl	$PSL_AC,%eax
	pushl	%ecx
	popfl

	testl	%eax,%eax
	jnz	try486

	/*
	 * Try the test of a NexGen CPU -- ZF will not change on a DIV
	 * instruction on a NexGen, it will on an i386.  Documented in
	 * Nx586 Processor Recognition Application Note, NexGen, Inc.
	 */
	movl	$0x5555,%eax
	xorl	%edx,%edx
	movl	$2,%ecx
	divl	%ecx
	jnz	is386

isnx586:
	/*
	 * Don't try cpuid, as Nx586s reportedly don't support the
	 * PSL_ID bit.
	 */
	movl	$CPU_NX586,RELOC(cputype)
	jmp	2f

is386:
	movl	$CPU_386,RELOC(cputype)
	jmp	2f

try486:	/* Try to toggle identification flag; does not exist on early 486s. */
	pushfl
	popl	%eax
	movl	%eax,%ecx
	xorl	$PSL_ID,%eax
	pushl	%eax
	popfl
	pushfl
	popl	%eax
	xorl	%ecx,%eax
	andl	$PSL_ID,%eax
	pushl	%ecx
	popfl

	testl	%eax,%eax
	jnz	try586
is486:	movl	$CPU_486,RELOC(cputype)
	/*
	 * Check Cyrix CPU
	 * Cyrix CPUs do not change the undefined flags following
	 * execution of the divide instruction which divides 5 by 2.
	 *
	 * Note: CPUID is enabled on M2, so it passes another way.
	 */
	pushfl
	movl	$0x5555, %eax
	xorl	%edx, %edx
	movl	$2, %ecx
	clc
	divl	%ecx
	jnc	trycyrix486
	popfl
	jmp 2f
trycyrix486:
	movl	$CPU_6x86,RELOC(cputype)	# set CPU type
	/*
	 * Check for Cyrix 486 CPU by seeing if the flags change during a
	 * divide. This is documented in the Cx486SLC/e SMM Programmer's
	 * Guide.
	 */
	xorl	%edx,%edx
	cmpl	%edx,%edx		# set flags to known state
	pushfl
	popl	%ecx			# store flags in ecx
	movl	$-1,%eax
	movl	$4,%ebx
	divl	%ebx			# do a long division
	pushfl
	popl	%eax
	xorl	%ecx,%eax		# are the flags different?
	testl	$0x8d5,%eax		# only check C|PF|AF|Z|N|V
	jne	2f			# yes; must be Cyrix 6x86 CPU
	movl	$CPU_486DLC,RELOC(cputype) 	# set CPU type

#ifndef CYRIX_CACHE_WORKS
	/* Disable caching of the ISA hole only. */
	invd
	movb	$CCR0,%al		# Configuration Register index (CCR0)
	outb	%al,$0x22
	inb	$0x23,%al
	orb	$(CCR0_NC1|CCR0_BARB),%al
	movb	%al,%ah
	movb	$CCR0,%al
	outb	%al,$0x22
	movb	%ah,%al
	outb	%al,$0x23
	invd
#else /* CYRIX_CACHE_WORKS */
	/* Set cache parameters */
	invd				# Start with guaranteed clean cache
	movb	$CCR0,%al		# Configuration Register index (CCR0)
	outb	%al,$0x22
	inb	$0x23,%al
	andb	$~CCR0_NC0,%al
#ifndef CYRIX_CACHE_REALLY_WORKS
	orb	$(CCR0_NC1|CCR0_BARB),%al
#else
	orb	$CCR0_NC1,%al
#endif
	movb	%al,%ah
	movb	$CCR0,%al
	outb	%al,$0x22
	movb	%ah,%al
	outb	%al,$0x23
	/* clear non-cacheable region 1	*/
	movb	$(NCR1+2),%al
	outb	%al,$0x22
	movb	$NCR_SIZE_0K,%al
	outb	%al,$0x23
	/* clear non-cacheable region 2	*/
	movb	$(NCR2+2),%al
	outb	%al,$0x22
	movb	$NCR_SIZE_0K,%al
	outb	%al,$0x23
	/* clear non-cacheable region 3	*/
	movb	$(NCR3+2),%al
	outb	%al,$0x22
	movb	$NCR_SIZE_0K,%al
	outb	%al,$0x23
	/* clear non-cacheable region 4	*/
	movb	$(NCR4+2),%al
	outb	%al,$0x22
	movb	$NCR_SIZE_0K,%al
	outb	%al,$0x23
	/* enable caching in CR0 */
	movl	%cr0,%eax
	andl	$~(CR0_CD|CR0_NW),%eax
	movl	%eax,%cr0
	invd
#endif /* CYRIX_CACHE_WORKS */

	jmp	2f

try586:	/* Use the `cpuid' instruction. */
	xorl	%eax,%eax
	cpuid
	movl	%eax,RELOC(cpuid_level)

2:
	/*
	 * Finished with old stack; load new %esp now instead of later so we
	 * can trace this code without having to worry about the trace trap
	 * clobbering the memory test or the zeroing of the bss+bootstrap page
	 * tables.
	 *
	 * The boot program should check:
	 *	text+data <= &stack_variable - more_space_for_stack
	 *	text+data+bss+pad+space_for_page_tables <= end_of_memory
	 * Oops, the gdt is in the carcass of the boot program so clearing
	 * the rest of memory is still not possible.
	 */
	movl	$_RELOC(tmpstk),%esp	# bootstrap stack end location

/*
 * Virtual address space of kernel, without PAE. The page dir is 1 page long.
 *
 * text | data | bss | [syms] | [blobs] | page dir | proc0 kstack | L1 ptp
 *			                0          1       2      3
 *
 * Virtual address space of kernel, with PAE. We need 4 pages for the page dir
 * and 1 page for the L3.
 * text | data | bss | [syms] | [blobs] | L3 | page dir | proc0 kstack | L1 ptp
 * 					0    1          5       6      7
 */
#ifndef PAE
#define	PROC0_PDIR_OFF	0
#else
#define PROC0_L3_OFF	0
#define PROC0_PDIR_OFF	1 * PAGE_SIZE
#endif

#define	PROC0_STK_OFF	(PROC0_PDIR_OFF + PDP_SIZE * PAGE_SIZE)
#define	PROC0_PTP1_OFF	(PROC0_STK_OFF + UPAGES * PAGE_SIZE)

/*
 * fillkpt - Fill in a kernel page table
 *	eax = pte (page frame | control | status)
 *	ebx = page table address
 *	ecx = number of pages to map
 *
 * For PAE, each entry is 8 bytes long: we must set the 4 upper bytes to 0.
 * This is done by the first instruction of fillkpt. In the non-PAE case, this
 * instruction just clears the page table entry.
 */

#define fillkpt	\
1:	movl	$0,(PDE_SIZE-4)(%ebx)	;	/* clear bits */	\
	movl	%eax,(%ebx)		;	/* store phys addr */	\
	addl	$PDE_SIZE,%ebx		;	/* next pte/pde */	\
	addl	$PAGE_SIZE,%eax		;	/* next phys page */	\
	loop	1b			;

	/* Find end of kernel image. */
	movl	$RELOC(end),%edi

#if (NKSYMS || defined(DDB) || defined(MODULAR)) && !defined(makeoptions_COPY_SYMTAB)
	/* Save the symbols (if loaded). */
	movl	RELOC(esym),%eax
	testl	%eax,%eax
	jz	1f
	subl	$KERNBASE,%eax
	movl	%eax,%edi
1:
#endif

	/* Skip over any modules/blobs. */
	movl	RELOC(eblob),%eax
	testl	%eax,%eax
	jz	1f
	subl	$KERNBASE,%eax
	movl	%eax,%edi
1:
	/* Compute sizes */
	movl	%edi,%esi
	addl	$PGOFSET,%esi			# page align up
	andl	$~PGOFSET,%esi

	/* nkptp[1] = (esi + ~L2_FRAME) >> L2_SHIFT + 1; */
	movl	%esi,%eax
	addl	$~L2_FRAME,%eax
	shrl	$L2_SHIFT,%eax
	incl	%eax		/* one more ptp for VAs stolen by bootstrap */
1:	movl	%eax,RELOC(nkptp)+1*4

	/* tablesize = (PDP_SIZE + UPAGES + nkptp) << PGSHIFT; */
	addl	$(PDP_SIZE+UPAGES),%eax
#ifdef PAE
	incl	%eax 		/* one more page for the L3 PD */
	shll	$PGSHIFT+1,%eax	/* PTP tables are twice larger with PAE */
#else
	shll	$PGSHIFT,%eax
#endif
	movl	%eax,RELOC(tablesize)

	/* ensure that nkptp covers bootstrap tables */
	addl	%esi,%eax
	addl	$~L2_FRAME,%eax
	shrl	$L2_SHIFT,%eax
	incl	%eax
	cmpl	%eax,RELOC(nkptp)+1*4
	jnz	1b

	/* Clear tables */
	movl	%esi,%edi
	xorl	%eax,%eax
	cld
	movl	RELOC(tablesize),%ecx
	shrl	$2,%ecx
	rep
	stosl

	leal	(PROC0_PTP1_OFF)(%esi), %ebx

/*
 * Build initial page tables.
 */
	/*
	 * Compute &__data_start - KERNBASE. This can't be > 4G,
	 * or we can't deal with it anyway, since we can't load it in
	 * 32 bit mode. So use the bottom 32 bits.
	 */
	movl	$RELOC(__data_start),%edx
	andl	$~PGOFSET,%edx

	/*
	 * Skip the first MB.
	 */
	movl	$_RELOC(KERNTEXTOFF),%eax
	movl	%eax,%ecx
	shrl	$(PGSHIFT-2),%ecx	/* ((n >> PGSHIFT) << 2) for # pdes */
#ifdef PAE
	shll	$1,%ecx			/* pdes are twice larger with PAE */
#endif
	addl	%ecx,%ebx

	/* Map the kernel text read-only. */
	movl	%edx,%ecx
	subl	%eax,%ecx
	shrl	$PGSHIFT,%ecx
	orl	$(PG_V|PG_KR),%eax
	fillkpt

	/* Map the data, BSS, and bootstrap tables read-write. */
	leal	(PG_V|PG_KW)(%edx),%eax
	movl	RELOC(tablesize),%ecx
	addl	%esi,%ecx				# end of tables
	subl	%edx,%ecx				# subtract end of text
	shrl	$PGSHIFT,%ecx
	fillkpt

	/* Map ISA I/O mem (later atdevbase) */
	movl	$(IOM_BEGIN|PG_V|PG_KW/*|PG_N*/),%eax	# having these bits set
	movl	$(IOM_SIZE>>PGSHIFT),%ecx		# for this many pte s,
	fillkpt

/*
 * Construct a page table directory.
 */
	/* Set up top level entries for identity mapping */
	leal	(PROC0_PDIR_OFF)(%esi),%ebx
	leal	(PROC0_PTP1_OFF)(%esi),%eax
	orl	$(PG_V|PG_KW), %eax
	movl	RELOC(nkptp)+1*4,%ecx
	fillkpt

	/* Set up top level entries for actual kernel mapping */
	leal	(PROC0_PDIR_OFF + L2_SLOT_KERNBASE*PDE_SIZE)(%esi),%ebx
	leal	(PROC0_PTP1_OFF)(%esi),%eax
	orl	$(PG_V|PG_KW), %eax
	movl	RELOC(nkptp)+1*4,%ecx
	fillkpt

	/* Install a PDE recursively mapping page directory as a page table! */
	leal	(PROC0_PDIR_OFF + PDIR_SLOT_PTE*PDE_SIZE)(%esi),%ebx
	leal	(PROC0_PDIR_OFF)(%esi),%eax
	orl	$(PG_V|PG_KW),%eax
	movl	$PDP_SIZE,%ecx
	fillkpt

#ifdef PAE
	/* Fill in proc0 L3 page with entries pointing to the page dirs */
	leal	(PROC0_L3_OFF)(%esi),%ebx
	leal	(PROC0_PDIR_OFF)(%esi),%eax
	orl	$(PG_V),%eax
	movl	$PDP_SIZE,%ecx
	fillkpt

	/* Enable PAE mode */
	movl	%cr4,%eax
	orl	$CR4_PAE,%eax
	movl	%eax,%cr4
#endif

	/* Save phys. addr of PDP, for libkvm. */
	leal	(PROC0_PDIR_OFF)(%esi),%eax
	movl	%eax,RELOC(PDPpaddr)

	/*
	 * Startup checklist:
	 * 1. Load %cr3 with pointer to PDIR (or L3 PD page for PAE).
	 */
	movl	%esi,%eax		# phys address of ptd in proc 0
	movl	%eax,%cr3		# load ptd addr into mmu

	/*
	 * 2. Enable paging and the rest of it.
	 */
	movl	%cr0,%eax		# get control word
					# enable paging & NPX
	orl	$(CR0_PE|CR0_PG|CR0_NE|CR0_TS|CR0_MP|CR0_WP|CR0_AM),%eax
	movl	%eax,%cr0		# and page NOW!

	pushl	$begin			# jump to high mem
	ret

begin:
	/*
	 * We have arrived.
	 * There's no need anymore for the identity mapping in low
	 * memory, remove it.
	 */
	movl	_C_LABEL(nkptp)+1*4,%ecx
	leal	(PROC0_PDIR_OFF)(%esi),%ebx	# old, phys address of PDIR
	addl	$(KERNBASE), %ebx		# new, virtual address of PDIR
1:	movl	$0,(PDE_SIZE-4)(%ebx)		# Upper bits (for PAE)
	movl	$0,(%ebx)
	addl	$PDE_SIZE,%ebx
	loop	1b

	/* Relocate atdevbase. */
	movl	$KERNBASE,%edx
	addl	_C_LABEL(tablesize),%edx
	addl	%esi,%edx
	movl	%edx,_C_LABEL(atdevbase)

	/* Set up bootstrap stack. */
	leal	(PROC0_STK_OFF+KERNBASE)(%esi),%eax
	movl	%eax,_C_LABEL(lwp0uarea)
	leal	(USPACE-FRAMESIZE)(%eax),%esp
	movl	%esi,PCB_CR3(%eax)	# pcb->pcb_cr3
	xorl	%ebp,%ebp		# mark end of frames

#if defined(MULTIBOOT)
	/* It is now safe to parse the Multiboot information structure
	 * we saved before from C code.  Note that we cannot delay its
	 * parsing any more because initgdt (called below) needs to make
	 * use of this information. */
	call	_C_LABEL(multiboot_post_reloc)
#endif

	subl	$NGDT*8, %esp		# space for temporary gdt
	pushl	%esp
	call	_C_LABEL(initgdt)
	addl	$4,%esp

	movl	_C_LABEL(tablesize),%eax
	addl	%esi,%eax		# skip past stack and page tables

#ifdef PAE
	pushl	$0	# init386() expects a 64 bits paddr_t with PAE
#endif
	pushl	%eax
	call	_C_LABEL(init386)	# wire 386 chip for unix operation
	addl	$PDE_SIZE,%esp		# pop paddr_t
	addl	$NGDT*8,%esp		# pop temporary gdt

#ifdef SAFARI_FIFO_HACK
	movb	$5,%al
	movw	$0x37b,%dx
	outb	%al,%dx
	movw	$0x37f,%dx
	inb	%dx,%al
	movb	%al,%cl

	orb	$1,%cl

	movb	$5,%al
	movw	$0x37b,%dx
	outb	%al,%dx
	movw	$0x37f,%dx
	movb	%cl,%al
	outb	%al,%dx
#endif /* SAFARI_FIFO_HACK */

	call 	_C_LABEL(main)
#else /* XEN */
	/* First, reset the PSL. */
	pushl	$PSL_MBO
	popfl

	cld
	movl	%esp, %ebx		# save start of available space
	movl	$_RELOC(tmpstk),%esp	# bootstrap stack end location

	/* Clear BSS. */
	xorl	%eax,%eax
	movl	$RELOC(__bss_start),%edi
	movl	$RELOC(_end),%ecx
	subl	%edi,%ecx
	rep stosb

	/* Copy the necessary stuff from start_info structure. */
	/* We need to copy shared_info early, so that sti/cli work */
	movl	$RELOC(start_info_union),%edi
	movl	$128,%ecx
	rep movsl

	/* Clear segment registers; always null in proc0. */
	xorl	%eax,%eax
	movw	%ax,%fs
	movw	%ax,%gs
	decl	%eax
	movl	%eax,RELOC(cpuid_level)

	xorl	%eax,%eax
	cpuid
	movl	%eax,RELOC(cpuid_level)

	/*
	 * Use a temp page. We'll re- add it to uvm(9) once we're
	 * done using it.
	 */
	movl	$RELOC(tmpgdt), %eax
	pushl	%eax		# start of temporary gdt
	call	_C_LABEL(initgdt)
	addl	$4,%esp

	call	xen_pmap_bootstrap

	/*
	 * First avail returned by xen_pmap_bootstrap in %eax
	 */
	movl	%eax, %esi;
	movl	%esi, _C_LABEL(lwp0uarea)

	/* Set up bootstrap stack. */
	leal	(USPACE-FRAMESIZE)(%eax),%esp
	xorl	%ebp,%ebp		# mark end of frames

	addl	$USPACE, %esi
	subl	$KERNBASE, %esi		#init386 want a physical address

#ifdef PAE
	pushl	$0	# init386() expects a 64 bits paddr_t with PAE
#endif
	pushl	%esi
	call	_C_LABEL(init386)	# wire 386 chip for unix operation
	addl	$PDE_SIZE,%esp		# pop paddr_t
	call 	_C_LABEL(main)
#endif /* XEN */
END(start)

#if defined(XEN)
/* space for the hypercall call page */
#define HYPERCALL_PAGE_OFFSET 0x1000
.org HYPERCALL_PAGE_OFFSET
ENTRY(hypercall_page)
.skip 0x1000
END(hypercall_page)

/*
 * void lgdt_finish(void);
 * Finish load a new GDT pointer (do any necessary cleanup).
 * XXX It's somewhat questionable whether reloading all the segment registers
 * is necessary, since the actual descriptor data is not changed except by
 * process creation and exit, both of which clean up via task switches.  OTOH,
 * this only happens at run time when the GDT is resized.
 */
/* LINTSTUB: Func: void lgdt_finish(void) */
NENTRY(lgdt_finish)
	movl	$GSEL(GDATA_SEL, SEL_KPL),%eax
	movw	%ax,%ds
	movw	%ax,%es
	movw	%ax,%gs
	movw	%ax,%ss
	movl	$GSEL(GCPU_SEL, SEL_KPL),%eax
	movw	%ax,%fs
	/* Reload code selector by doing intersegment return. */
	popl	%eax
	pushl	$GSEL(GCODE_SEL, SEL_KPL)
	pushl	%eax
	lret
END(lgdt_finish)

#endif /* XEN */

/*
 * void lwp_trampoline(void);
 *
 * This is a trampoline function pushed onto the stack of a newly created
 * process in order to do some additional setup.  The trampoline is entered by
 * cpu_switchto()ing to the process, so we abuse the callee-saved
 * registers used by cpu_switchto() to store the information about the
 * stub to call.
 * NOTE: This function does not have a normal calling sequence!
 */
NENTRY(lwp_trampoline)
	movl	%ebp,%edi	/* for .Lsyscall_checkast */
	xorl	%ebp,%ebp
	pushl	%edi
	pushl	%eax
	call	_C_LABEL(lwp_startup)
	addl	$8,%esp
	pushl	%ebx
	call	*%esi
	addl	$4,%esp
	jmp	.Lsyscall_checkast
	/* NOTREACHED */
END(lwp_trampoline)

/*
 * sigcode()
 *
 * Signal trampoline; copied to top of user stack.  Used only for
 * compatibility with old releases of NetBSD.
 */
NENTRY(sigcode)
	/*
	 * Handler has returned here as if we called it.  The sigcontext
	 * is on the stack after the 3 args "we" pushed.
	 */
	leal	12(%esp),%eax		# get pointer to sigcontext
	movl	%eax,4(%esp)		# put it in the argument slot
					# fake return address already there
	movl	$SYS_compat_16___sigreturn14,%eax
	int	$0x80	 		# enter kernel with args on stack
	movl	$SYS_exit,%eax
	int	$0x80			# exit if sigreturn fails
	.globl	_C_LABEL(esigcode)
_C_LABEL(esigcode):
END(sigcode)

/*
 * int setjmp(label_t *)
 *
 * Used primarily by DDB.
 */
ENTRY(setjmp)
	movl	4(%esp),%eax
	movl	%ebx,(%eax)		# save ebx
	movl	%esp,4(%eax)		# save esp
	movl	%ebp,8(%eax)		# save ebp
	movl	%esi,12(%eax)		# save esi
	movl	%edi,16(%eax)		# save edi
	movl	(%esp),%edx		# get rta
	movl	%edx,20(%eax)		# save eip
	xorl	%eax,%eax		# return 0
	ret
END(setjmp)

/*
 * int longjmp(label_t *)
 *
 * Used primarily by DDB.
 */
ENTRY(longjmp)
	movl	4(%esp),%eax
	movl	(%eax),%ebx		# restore ebx
	movl	4(%eax),%esp		# restore esp
	movl	8(%eax),%ebp		# restore ebp
	movl	12(%eax),%esi		# restore esi
	movl	16(%eax),%edi		# restore edi
	movl	20(%eax),%edx		# get rta
	movl	%edx,(%esp)		# put in return frame
	movl	$1,%eax			# return 1
	ret
END(longjmp)

/*
 * void dumpsys(void)
 *
 * Mimic cpu_switchto() for postmortem debugging.
 */
ENTRY(dumpsys)
	pushl	%ebx			# set up fake switchframe
	pushl	%esi			#   and save context
	pushl	%edi			
	movl	%esp,_C_LABEL(dumppcb)+PCB_ESP
	movl	%ebp,_C_LABEL(dumppcb)+PCB_EBP
	call	_C_LABEL(dodumpsys)	# dump!
	addl	$(3*4), %esp		# unwind switchframe
	ret
END(dumpsys)

/*
 * struct lwp *cpu_switchto(struct lwp *oldlwp, struct lwp *newlwp,
 *			    bool returning)
 *
 *	1. if (oldlwp != NULL), save its context.
 *	2. then, restore context of newlwp.
 *
 * Note that the stack frame layout is known to "struct switchframe" in
 * <machine/frame.h> and to the code in cpu_lwp_fork() which initializes
 * it for a new lwp.
 */
ENTRY(cpu_switchto)
	pushl	%ebx
	pushl	%esi
	pushl	%edi

#if defined(DIAGNOSTIC) && !defined(XEN)
	cmpl	$IPL_SCHED,CPUVAR(ILEVEL)
	jbe	0f
	pushl	CPUVAR(ILEVEL)
	pushl	$.Lstr
	call	_C_LABEL(panic)
	addl	$8,%esp
.Lstr:	.string	"cpu_switchto: switching above IPL_SCHED (%d)\0"
0:
#endif

	movl	16(%esp),%esi		# oldlwp
	movl	20(%esp),%edi		# newlwp
	movl	24(%esp),%edx		# returning
	testl	%esi,%esi
	jz	1f

	/* Save old context. */
	movl	L_PCB(%esi),%eax
	movl	%esp,PCB_ESP(%eax)
	movl	%ebp,PCB_EBP(%eax)

	/* Switch to newlwp's stack. */
1:	movl	L_PCB(%edi),%ebx
	movl	PCB_EBP(%ebx),%ebp
	movl	PCB_ESP(%ebx),%esp

	/*
	 * Set curlwp.  This must be globally visible in order to permit
	 * non-interlocked mutex release.
	 */
	movl	%edi,%ecx
	xchgl	%ecx,CPUVAR(CURLWP)

	/* Skip the rest if returning to a pinned LWP. */
	testl	%edx,%edx
	jnz	4f

#ifdef XEN
	pushl	%edi
	call	_C_LABEL(i386_switch_context)
	addl	$4,%esp
#else /* !XEN */
	/* Switch ring0 esp */
	movl	PCB_ESP0(%ebx),%eax
	movl	%eax,CPUVAR(ESP0)
#endif /* !XEN */

	/* Don't bother with the rest if switching to a system process. */
	testl	$LW_SYSTEM,L_FLAG(%edi)
	jnz	4f

#ifndef XEN
	/* Restore thread-private %fs/%gs descriptors. */
	movl	CPUVAR(GDT),%ecx
	movl	PCB_FSD(%ebx), %eax
	movl	PCB_FSD+4(%ebx), %edx
	movl	%eax, (GUFS_SEL*8)(%ecx)
	movl	%edx, (GUFS_SEL*8+4)(%ecx)
	movl	PCB_GSD(%ebx), %eax
	movl	PCB_GSD+4(%ebx), %edx
	movl	%eax, (GUGS_SEL*8)(%ecx)
	movl	%edx, (GUGS_SEL*8+4)(%ecx)
#endif /* !XEN */

	/* Switch I/O bitmap */
	movl	PCB_IOMAP(%ebx),%eax
	orl	%eax,%eax
	jnz	.Lcopy_iobitmap
	movl	$(IOMAP_INVALOFF << 16),CPUVAR(IOBASE)
.Liobitmap_done:

	/* Is this process using RAS (restartable atomic sequences)? */
	movl	L_PROC(%edi),%eax
	cmpl	$0,P_RASLIST(%eax)
	jne	5f

	/*
	 * Restore cr0 (including FPU state).  Raise the IPL to IPL_HIGH.
	 * FPU IPIs can alter the LWP's saved cr0.  Dropping the priority
	 * is deferred until mi_switch(), when cpu_switchto() returns.
	 */
2:
#ifdef XEN
	pushl	%edi
	call	_C_LABEL(i386_tls_switch)
	addl	$4,%esp
#else /* !XEN */
	movl	$IPL_HIGH,CPUVAR(ILEVEL)
	movl	PCB_CR0(%ebx),%ecx		/* has CR0_TS clear */
	movl	%cr0,%edx

	/*
	 * If our floating point registers are on a different CPU,
	 * set CR0_TS so we'll trap rather than reuse bogus state.
	 */
	cmpl	CPUVAR(FPCURLWP),%edi
	je	3f
	orl	$CR0_TS,%ecx

	/* Reloading CR0 is very expensive - avoid if possible. */
3:	cmpl	%edx,%ecx
	je	4f
	movl	%ecx,%cr0
#endif /* !XEN */

	/* Return to the new LWP, returning 'oldlwp' in %eax. */
4:	movl	%esi,%eax
	popl	%edi
	popl	%esi
	popl	%ebx
	ret

	/* Check for restartable atomic sequences (RAS). */
5:	movl	L_MD_REGS(%edi),%ecx
	pushl	TF_EIP(%ecx)
	pushl	%eax
	call	_C_LABEL(ras_lookup)
	addl	$8,%esp
	cmpl	$-1,%eax
	je	2b
	movl	L_MD_REGS(%edi),%ecx
	movl	%eax,TF_EIP(%ecx)
	jmp	2b

.Lcopy_iobitmap:
	/* Copy I/O bitmap. */
	incl	_C_LABEL(pmap_iobmp_evcnt)+EV_COUNT
	movl	$(IOMAPSIZE/4),%ecx
	pushl	%esi
	pushl	%edi
	movl	%eax,%esi		/* pcb_iomap */
	movl	CPUVAR(SELF),%edi
	leal	CPU_INFO_IOMAP(%edi),%edi
	rep
	movsl
	popl	%edi
	popl	%esi
	movl	$((CPU_INFO_IOMAP - CPU_INFO_TSS) << 16),CPUVAR(IOBASE)
	jmp	.Liobitmap_done
END(cpu_switchto)

/*
 * void savectx(struct pcb *pcb);
 *
 * Update pcb, saving current processor state.
 */
ENTRY(savectx)
	movl	4(%esp),%edx		# edx = pcb
	movl	%esp,PCB_ESP(%edx)
	movl	%ebp,PCB_EBP(%edx)
	ret
END(savectx)

/*
 * osyscall()
 *
 * Old call gate entry for syscall
 */
IDTVEC(osyscall)
#ifndef XEN
	/* XXX we are in trouble! interrupts be off here. */
	cli			# must be first instruction
#endif
	pushfl			# set eflags in trap frame
	popl	8(%esp)
	orl	$PSL_I,8(%esp)	# re-enable ints on return to user
	pushl	$7		# size of instruction for restart
	jmp	syscall1
IDTVEC_END(osyscall)

/*
 * syscall()
 *
 * Trap gate entry for syscall
 */
IDTVEC(syscall)
	pushl	$2		# size of instruction for restart
syscall1:
	pushl	$T_ASTFLT	# trap # for doing ASTs
	INTRENTRY
	STI(%eax)
#ifdef DIAGNOSTIC
	movl	CPUVAR(ILEVEL),%ebx
	testl	%ebx,%ebx
	jz	1f
	pushl	$5f
	call	_C_LABEL(panic)
	addl	$4,%esp
#ifdef DDB
	int	$3
#endif
1:
#endif /* DIAGNOSTIC */
	addl	$1,CPUVAR(NSYSCALL)	# count it atomically
	adcl	$0,CPUVAR(NSYSCALL)+4	# count it atomically
	movl	CPUVAR(CURLWP),%edi
	movl	L_PROC(%edi),%edx
	movl	%esp,L_MD_REGS(%edi)	# save pointer to frame
	pushl	%esp
	call	*P_MD_SYSCALL(%edx)	# get pointer to syscall() function
	addl	$4,%esp
.Lsyscall_checkast:
	/* Check for ASTs on exit to user mode. */
	CLI(%eax)
	movl	L_MD_ASTPENDING(%edi), %eax
	orl	CPUVAR(WANT_PMAPLOAD), %eax
	jnz	9f
#ifdef XEN
	STIC(%eax)
	jz	14f
	call	_C_LABEL(stipending)
	testl	%eax,%eax
	jz	14f
	/* process pending interrupts */
	CLI(%eax)
	movl	CPUVAR(ILEVEL), %ebx
	movl	$.Lsyscall_resume, %esi # address to resume loop at
.Lsyscall_resume:
	movl	%ebx,%eax	# get cpl
	movl	CPUVAR(IUNMASK)(,%eax,4),%eax
	andl	CPUVAR(IPENDING),%eax	# any non-masked bits left?
	jz	17f
	bsrl	%eax,%eax
	btrl	%eax,CPUVAR(IPENDING)
	movl	CPUVAR(ISOURCES)(,%eax,4),%eax
	jmp	*IS_RESUME(%eax)
17:	movl	%ebx, CPUVAR(ILEVEL)	#restore cpl
	jmp	.Lsyscall_checkast
14:
#endif /* XEN */
#ifndef DIAGNOSTIC
	INTRFASTEXIT
#else /* DIAGNOSTIC */
	cmpl	$IPL_NONE,CPUVAR(ILEVEL)
	jne	3f
	INTRFASTEXIT
3:	STI(%eax)
	pushl	$4f
	call	_C_LABEL(panic)
	addl	$4,%esp
	pushl	$IPL_NONE
	call	_C_LABEL(spllower)
	addl	$4,%esp
	jmp	.Lsyscall_checkast
4:	.asciz	"SPL NOT LOWERED ON SYSCALL EXIT\n"
5:	.asciz	"SPL NOT ZERO ON SYSCALL ENTRY\n"
#endif /* DIAGNOSTIC */
9:
	cmpl	$0, CPUVAR(WANT_PMAPLOAD)
	jz	10f
	STI(%eax)
	call	_C_LABEL(pmap_load)
	jmp	.Lsyscall_checkast	/* re-check ASTs */
10:
	/* Always returning to user mode here. */
	movl	$0, L_MD_ASTPENDING(%edi)
	STI(%eax)
	/* Pushed T_ASTFLT into tf_trapno on entry. */
	pushl	%esp
	call	_C_LABEL(trap)
	addl	$4,%esp
	jmp	.Lsyscall_checkast	/* re-check ASTs */
IDTVEC_END(syscall)

IDTVEC(svr4_fasttrap)
	pushl	$2		# size of instruction for restart
	pushl	$T_ASTFLT	# trap # for doing ASTs
	INTRENTRY
	STI(%eax)
	pushl	$RW_READER
	pushl	$_C_LABEL(svr4_fasttrap_lock)
	call	_C_LABEL(rw_enter)
	addl	$8,%esp
	call	*_C_LABEL(svr4_fasttrap_vec)
	pushl	$_C_LABEL(svr4_fasttrap_lock)
	call	_C_LABEL(rw_exit)
	addl	$4,%esp
2:	/* Check for ASTs on exit to user mode. */
	cli
	CHECK_ASTPENDING(%eax)		
	je	1f
	/* Always returning to user mode here. */
	CLEAR_ASTPENDING(%eax)
	sti
	/* Pushed T_ASTFLT into tf_trapno on entry. */
	pushl	%esp
	call	_C_LABEL(trap)
	addl	$4,%esp
	jmp	2b
1:	CHECK_DEFERRED_SWITCH
	jnz	9f
	INTRFASTEXIT
9:	sti
	call	_C_LABEL(pmap_load)
	cli
	jmp	2b

/*
 * int npx586bug1(int a, int b)
 * Used when checking for the FDIV bug on first generations pentiums.
 * Anything 120MHz or above is fine.
 */
NENTRY(npx586bug1)
	fildl	4(%esp)		# x
	fildl	8(%esp)		# y
	fld	%st(1)
	fdiv	%st(1),%st	# x/y
	fmulp	%st,%st(1)	# (x/y)*y
	fsubrp	%st,%st(1)	# x-(x/y)*y
	pushl	$0
	fistpl	(%esp)
	popl	%eax
	ret
END(npx586bug1)

/*
 * void sse2_idlezero_page(void *pg)
 *
 * Zero a page without polluting the cache.  Preemption must be
 * disabled by the caller. Abort if a preemption is pending.
 */
ENTRY(sse2_idlezero_page)
	pushl	%ebp
	movl	%esp,%ebp
	movl	8(%esp), %edx
	movl	$(PAGE_SIZE/32), %ecx
	xorl	%eax, %eax
	.align	16
1:
	testl	$RESCHED_KPREEMPT, CPUVAR(RESCHED)
	jnz	2f
	movnti	%eax, 0(%edx)
	movnti	%eax, 4(%edx)
	movnti	%eax, 8(%edx)
	movnti	%eax, 12(%edx)
	movnti	%eax, 16(%edx)
	movnti	%eax, 20(%edx)
	movnti	%eax, 24(%edx)
	movnti	%eax, 28(%edx)
	addl	$32, %edx
	decl	%ecx
	jnz	1b
	sfence
	incl	%eax
	pop	%ebp
	ret
2:
	sfence
	popl	%ebp
	ret
END(sse2_idlezero_page)
