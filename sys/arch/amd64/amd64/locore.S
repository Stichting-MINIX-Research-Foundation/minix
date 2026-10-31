/*	$NetBSD: locore.S,v 1.214.4.5 2026/01/25 16:38:53 martin Exp $	*/

/*
 * Copyright-o-rama!
 */

/*
 * Copyright (c) 1998, 2000, 2007, 2008, 2016 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum and by Maxime Villard.
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

/*
 * Copyright (c) 2007 Manuel Bouyer.
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
 * Copyright (c) 2006 Mathieu Ropert <mro@adviseo.fr>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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

/* Override user-land alignment before including asm.h */
#define	ALIGN_DATA	.align	8
#define ALIGN_TEXT	.align 16,0x90
#define _ALIGN_TEXT	ALIGN_TEXT

#include <machine/asm.h>

#include "opt_kasan.h"
#include "opt_copy_symtab.h"
#include "opt_ddb.h"
#include "opt_ddbparam.h"
#include "opt_modular.h"
#include "opt_realmem.h"
#include "opt_selfreloc.h"

#include "opt_compat_netbsd.h"
#include "opt_compat_netbsd32.h"
#include "opt_xen.h"
#include "opt_svs.h"

#include "assym.h"
#include "lapic.h"
#include "ioapic.h"
#include "ksyms.h"

#include <sys/errno.h>
#include <sys/syscall.h>

#include <machine/pte.h>
#include <machine/segments.h>
#include <machine/specialreg.h>
#include <machine/trap.h>
#include <machine/bootinfo.h>
#include <machine/frameasm.h>
#include <machine/cputypes.h>

#if NLAPIC > 0
#include <machine/i82489reg.h>
#endif

/* Get definitions for IOM_BEGIN, IOM_END, and IOM_SIZE */
#include <dev/isa/isareg.h>

#define	_RELOC(x)	((x) - KERNBASE)
#define	RELOC(x)	_RELOC(_C_LABEL(x))

/* 32bit version of PTE_NX */
#define PTE_NX32	0x80000000

#if L2_SLOT_KERNBASE > 0
#define TABLE_L2_ENTRIES (2 * (NKL2_KIMG_ENTRIES + 1))
#else
#define TABLE_L2_ENTRIES (NKL2_KIMG_ENTRIES + 1)
#endif

#if L3_SLOT_KERNBASE > 0
#define TABLE_L3_ENTRIES (2 * NKL3_KIMG_ENTRIES)
#else
#define TABLE_L3_ENTRIES NKL3_KIMG_ENTRIES
#endif

#define PROC0_PML4_OFF	0
#define PROC0_STK_OFF	(PROC0_PML4_OFF + 1 * PAGE_SIZE)
#define PROC0_PTP3_OFF	(PROC0_STK_OFF + UPAGES * PAGE_SIZE)
#define PROC0_PTP2_OFF	(PROC0_PTP3_OFF + NKL4_KIMG_ENTRIES * PAGE_SIZE)
#define PROC0_PTP1_OFF	(PROC0_PTP2_OFF + TABLE_L3_ENTRIES * PAGE_SIZE)
#define TABLESIZE \
  ((NKL4_KIMG_ENTRIES + TABLE_L3_ENTRIES + TABLE_L2_ENTRIES + 1 + UPAGES) \
    * PAGE_SIZE)

/* Amount of VA used to map the kernel, the syms and the preloaded modules */
#define BOOTMAP_VA_SIZE \
	(NKL2_KIMG_ENTRIES * (1 << L2_SHIFT) - TABLESIZE - IOM_SIZE)

/*
 * fillkpt - Fill in a kernel page table
 *	eax = pte (page frame | control | status)
 *	ebx = page table address
 *	ecx = number of pages to map
 *
 * Each entry is 8 (PDE_SIZE) bytes long: we must set the 4 upper bytes to 0.
 */
#define fillkpt	\
	cmpl	$0,%ecx			;	/* zero-sized? */	\
	je 	2f			; \
1:	movl	$0,(PDE_SIZE-4)(%ebx)	;	/* upper 32 bits: 0 */	\
	movl	%eax,(%ebx)		;	/* store phys addr */	\
	addl	$PDE_SIZE,%ebx		;	/* next PTE/PDE */	\
	addl	$PAGE_SIZE,%eax		;	/* next phys page */	\
	loop	1b			; \
2:					;

/*
 * fillkpt_nox - Same as fillkpt, but sets the NX/XD bit.
 */
#define fillkpt_nox \
	cmpl	$0,%ecx			;	/* zero-sized? */	\
	je 	2f			; \
	pushl	%ebp			; \
	movl	RELOC(nox_flag),%ebp	; \
1:	movl	%ebp,(PDE_SIZE-4)(%ebx)	;	/* upper 32 bits: NX */ \
	movl	%eax,(%ebx)		;	/* store phys addr */	\
	addl	$PDE_SIZE,%ebx		;	/* next PTE/PDE */	\
	addl	$PAGE_SIZE,%eax		;	/* next phys page */	\
	loop	1b			; \
	popl	%ebp			; \
2:					;

/*
 * fillkpt_blank - Fill in a kernel page table with blank entries
 *	ebx = page table address
 *	ecx = number of pages to map
 */
#define fillkpt_blank	\
	cmpl	$0,%ecx			;	/* zero-sized? */	\
	je 	2f			; \
1:	movl	$0,(PDE_SIZE-4)(%ebx)	;	/* upper 32 bits: 0 */	\
	movl	$0,(%ebx)		;	/* lower 32 bits: 0 */	\
	addl	$PDE_SIZE,%ebx		;	/* next PTE/PDE */	\
	loop	1b			; \
2:					;

/*
 * killkpt - Destroy a kernel page table (long mode)
 *	rbx = page table address
 *	rcx = number of pages to destroy
 */
#define killkpt \
1:	movq	$0,(%rbx)	; \
	addq	$PDE_SIZE,%rbx	; \
	loop	1b		;


#ifdef XEN
#define __ASSEMBLY__
#include <xen/include/public/arch-x86/cpuid.h>
#include <xen/include/public/elfnote.h>
#include <xen/include/public/xen.h>

#define ELFNOTE(name, type, desctype, descdata...) \
.pushsection .note.name, "a", @note	;	\
  .align 4				;	\
  .long 2f - 1f		/* namesz */	;	\
  .long 4f - 3f		/* descsz */	;	\
  .long type				;	\
1:.asciz #name				;	\
2:.align 4				;	\
3:desctype descdata			;	\
4:.align 4				;	\
.popsection

/*
 * Xen guest identifier and loader selection
 */
.section __xen_guest
	ELFNOTE(Xen, XEN_ELFNOTE_GUEST_OS,       .asciz, "NetBSD")
	ELFNOTE(Xen, XEN_ELFNOTE_GUEST_VERSION,  .asciz, "4.99")
	ELFNOTE(Xen, XEN_ELFNOTE_XEN_VERSION,    .asciz, "xen-3.0")
	ELFNOTE(Xen, XEN_ELFNOTE_VIRT_BASE,      .quad,  KERNBASE)
#ifdef XENPV
	ELFNOTE(Xen, XEN_ELFNOTE_PADDR_OFFSET,   .quad,  KERNBASE)
	ELFNOTE(Xen, XEN_ELFNOTE_ENTRY,          .quad,  start)
#else
	ELFNOTE(Xen, XEN_ELFNOTE_PADDR_OFFSET,   .quad,  0)
	ELFNOTE(Xen, XEN_ELFNOTE_PHYS32_ENTRY,   .long,  RELOC(start_pvh))
#endif /* XENPV */
	ELFNOTE(Xen, XEN_ELFNOTE_HYPERCALL_PAGE, .quad,  hypercall_page)
	ELFNOTE(Xen, XEN_ELFNOTE_HV_START_LOW,   .quad,  HYPERVISOR_VIRT_START)
	ELFNOTE(Xen, XEN_ELFNOTE_FEATURES,       .asciz, "writable_descriptor_tables|auto_translated_physmap|supervisor_mode_kernel|hvm_callback_vector")
	ELFNOTE(Xen, XEN_ELFNOTE_PAE_MODE,       .asciz, "yes")
	ELFNOTE(Xen, XEN_ELFNOTE_L1_MFN_VALID,   .long,  PTE_P, PTE_P)\
	ELFNOTE(Xen, XEN_ELFNOTE_LOADER,         .asciz, "generic")     
	ELFNOTE(Xen, XEN_ELFNOTE_SUSPEND_CANCEL, .long,  0)
#if NKSYMS > 0 || defined(DDB) || defined(MODULAR)
	ELFNOTE(Xen, XEN_ELFNOTE_BSD_SYMTAB,     .asciz, "yes")
#endif
#endif  /* XEN */

/*
 * Initialization
 */
	.data

	.globl	_C_LABEL(tablesize)
	.globl	_C_LABEL(nox_flag)
	.globl	_C_LABEL(cputype)
	.globl	_C_LABEL(cpuid_level)
	.globl	_C_LABEL(esym)
	.globl	_C_LABEL(eblob)
	.globl	_C_LABEL(atdevbase)
	.globl	_C_LABEL(PDPpaddr)
	.globl	_C_LABEL(boothowto)
	.globl	_C_LABEL(bootinfo)
	.globl	_C_LABEL(biosbasemem)
	.globl	_C_LABEL(biosextmem)
	.globl	_C_LABEL(lwp0uarea)
	.globl	do_mov_es
	.globl	do_mov_ds
	.globl	do_mov_fs
	.globl	do_mov_gs
	.globl	do_iret

	.type	_C_LABEL(tablesize), @object
_C_LABEL(tablesize):	.long	TABLESIZE
END(tablesize)
	.type	_C_LABEL(nox_flag), @object
LABEL(nox_flag)		.long	0	/* 32bit NOX flag, set if supported */
END(nox_flag)
	.type	_C_LABEL(cputype), @object
LABEL(cputype)		.long	0	/* are we 80486, Pentium, or.. */
END(cputype)
	.type	_C_LABEL(cpuid_level), @object
LABEL(cpuid_level)	.long	-1	/* max. level accepted by cpuid instr */
END(cpuid_level)
	.type	_C_LABEL(esym), @object
LABEL(esym)		.quad	0	/* ptr to end of syms */
END(esym)
	.type	_C_LABEL(eblob), @object
LABEL(eblob)		.quad	0	/* ptr to end of modules */
END(eblob)
	.type	_C_LABEL(atdevbase), @object
LABEL(atdevbase)	.quad	0	/* location of start of iomem in virt */
END(atdevbase)
	.type	_C_LABEL(PDPpaddr), @object
LABEL(PDPpaddr)		.quad	0	/* paddr of PTD, for libkvm */
END(PDPpaddr)
	.type	_C_LABEL(biosbasemem), @object
#ifndef REALBASEMEM
LABEL(biosbasemem)	.long	0	/* base memory reported by BIOS */
#else
LABEL(biosbasemem)	.long	REALBASEMEM
#endif
END(biosbasemem)
	.type	_C_LABEL(biosextmem), @object
#ifndef REALEXTMEM
LABEL(biosextmem)	.long	0	/* extended memory reported by BIOS */
#else
LABEL(biosextmem)	.long	REALEXTMEM
#endif
END(biosextmem)
	.type	_C_LABEL(lwp0uarea), @object
LABEL(lwp0uarea)	.quad	0
END(lwp0uarea)

#ifndef XENPV
	.globl	gdt64_lo
	.globl	gdt64_hi

#define GDT64_LIMIT gdt64_end-gdt64_start-1
/* Temporary gdt64, with base address in low memory */
	.type	_C_LABEL(gdt64_lo), @object
LABEL(gdt64_lo)
	.word	GDT64_LIMIT
	.quad	_RELOC(gdt64_start)
END(gdt64_lo)
.align 64

/* Temporary gdt64, with base address in high memory */
	.type	_C_LABEL(gdt64_hi), @object
LABEL(gdt64_hi)
	.word	GDT64_LIMIT
	.quad	gdt64_start
END(gdt64_hi)
.align 64
#undef GDT64_LIMIT

	.type	_C_LABEL(gdt64_start), @object
_C_LABEL(gdt64_start):
	.quad 0x0000000000000000	/* always empty */
	.quad 0x00af9a000000ffff	/* kernel CS */
	.quad 0x00cf92000000ffff	/* kernel DS */
END(gdt64_start)
gdt64_end:

	.type	_C_LABEL(farjmp64), @object
_C_LABEL(farjmp64):
	.long	_RELOC(longmode)
	.word	GSEL(GCODE_SEL, SEL_KPL)
END(farjmp64)

#ifdef XEN
/* 32bit GDT */
gdtdesc32:
	.word	gdt32end - gdt32
	.long	RELOC(gdt32)
	.long	0
gdt32:  
	.long	0			# null descriptor
	.long	0
	.long	0x0000ffff		# %cs
	.long	0x00cf9a00 
	.long	0x0000ffff		# %ds, %es, %ss
	.long	0x00cf9200
gdt32end:
#endif /* XEN */
#endif	/* !XENPV */

	/* Space for the temporary stack */
	.size	tmpstk, tmpstk - .
	.space	512
tmpstk:

/*
 * Some hackage to deal with 64bit symbols in 32 bit mode.
 * This may not be needed if things are cleaned up a little.
 */

	.text
	.globl	_C_LABEL(kernel_text)
	.set	_C_LABEL(kernel_text),KERNTEXTOFF

ENTRY(start)
#ifndef XENPV
	.code32

#ifdef SELFRELOC
	call	next
next:	pop	%edi
	sub     $(next - kernel_text), %edi 

	/* If not KERNBASE, reloc ourselves to KERNBASE */
	cmpl	$(KERNTEXTOFF_LO - KERNBASE_LO), %edi
	jne	selfreloc_start
#endif /* SELFRELOC */

	/* Warm boot */
	movw	$0x1234,0x472

	/*
	 * Load parameters from the stack (32 bits):
	 *     boothowto, [bootdev], bootinfo, esym, biosextmem, biosbasemem
	 * We are not interested in 'bootdev'.
	 */

	/* Load 'boothowto' */
	movl	4(%esp),%eax
	movl	%eax,RELOC(boothowto)

	/* Load 'bootinfo' */
	movl	12(%esp),%eax
	testl	%eax,%eax		/* bootinfo = NULL? */
	jz	.Lbootinfo_finished

	movl	(%eax),%ebx		/* bootinfo::bi_nentries */
	movl	$RELOC(bootinfo),%ebp
	movl	%ebp,%edx
	addl	$BOOTINFO_MAXSIZE,%ebp
	movl	%ebx,(%edx)
	addl	$4,%edx

.Lbootinfo_entryloop:
	testl	%ebx,%ebx		/* no remaining entries? */
	jz	.Lbootinfo_finished

	addl	$4,%eax
	movl	(%eax),%ecx		/* address of entry */
	pushl	%edi
	pushl	%esi
	pushl	%eax

	movl	(%ecx),%eax		/* btinfo_common::len (size of entry) */
	movl	%edx,%edi
	addl	%eax,%edx		/* update dest pointer */
	cmpl	%ebp,%edx		/* beyond bootinfo+BOOTINFO_MAXSIZE? */
	jg	.Lbootinfo_overflow

	movl	%ecx,%esi
	movl	%eax,%ecx

	/*
	 * If any modules were loaded, record where they end. 'eblob' is used
	 * later to compute the initial bootstrap tables.
	 */
	cmpl	$BTINFO_MODULELIST,4(%esi) /* btinfo_common::type */
	jne	.Lbootinfo_copy

	/* Skip the modules if we won't have enough VA to map them */
	movl	12(%esi),%eax		/* btinfo_modulelist::endpa */
	addl	$PGOFSET,%eax		/* roundup to a page */
	andl	$~PGOFSET,%eax
	cmpl	$BOOTMAP_VA_SIZE,%eax
	jg	.Lbootinfo_skip
	movl	%eax,RELOC(eblob)
	addl	$KERNBASE_LO,RELOC(eblob)
	adcl	$KERNBASE_HI,RELOC(eblob)+4

.Lbootinfo_copy:
	rep
	movsb				/* copy esi -> edi */
	jmp	.Lbootinfo_next

.Lbootinfo_skip:
	subl	%ecx,%edx		/* revert dest pointer */

.Lbootinfo_next:
	popl	%eax
	popl	%esi
	popl	%edi
	subl	$1,%ebx			/* decrement the # of entries */
	jmp	.Lbootinfo_entryloop

.Lbootinfo_overflow:
	/*
	 * Cleanup for overflow case. Pop the registers, and correct the number
	 * of entries.
	 */
	popl	%eax
	popl	%esi
	popl	%edi
	movl	$RELOC(bootinfo),%ebp
	movl	%ebp,%edx
	subl	%ebx,(%edx)		/* correct the number of entries */
.Lbootinfo_finished:

	/* Load 'esym' */
	movl	16(%esp),%eax
	testl	%eax,%eax		/* esym = NULL? */
	jz	1f

	addl	$KERNBASE_LO,%eax

1:
	movl	$RELOC(esym),%ebp
	movl	%eax,(%ebp)
	movl	$KERNBASE_HI,4(%ebp)

	/* Load 'biosextmem' */
	movl	$RELOC(biosextmem),%ebp
	movl	(%ebp),%eax
	testl	%eax,%eax		/* already set? */
	jnz	.Lbiosextmem_finished

	movl	20(%esp),%eax
	movl	%eax,(%ebp)

.Lbiosextmem_finished:
	/* Load 'biosbasemem' */
	movl	$RELOC(biosbasemem),%ebp
	movl	(%ebp),%eax
	testl	%eax,%eax		/* already set? */
	jnz	.Lbiosbasemem_finished

	movl	24(%esp),%eax
	movl	%eax,(%ebp)

.Lbiosbasemem_finished:
	/*
	 * Done with the parameters!
	 */

	/* First, reset the PSL. */
	pushl	$PSL_MBO
	popfl

	xorl	%eax,%eax
	cpuid
	movl	%eax,RELOC(cpuid_level)

	/*
	 * Finished with old stack; load new %esp now instead of later so we
	 * can trace this code without having to worry about the trace trap
	 * clobbering the memory test or the zeroing of the bss+bootstrap page
	 * tables.
	 *
	 * The boot program should check:
	 *	text+data <= &stack_variable - more_space_for_stack
	 *	text+data+bss+pad+space_for_page_tables <= end_of_memory
	 *
	 * XXX: the gdt is in the carcass of the boot program so clearing
	 * the rest of memory is still not possible.
	 */
	movl	$RELOC(tmpstk),%esp

	/*
	 * Retrieve the NX/XD flag. We use the 32bit version of PTE_NX.
	 */
	movl	$0x80000001,%eax
	cpuid
	andl	$CPUID_NOX,%edx
	jz	.Lno_NOX
	movl	$PTE_NX32,RELOC(nox_flag)
.Lno_NOX:

/*
 * There are four levels of pages in amd64: PML4 -> PDP -> PD -> PT. They will
 * be referred to as: L4 -> L3 -> L2 -> L1.
 *
 * Virtual address space of the kernel:
 * +------+--------+------+-----+--------+---------------------+----------
 * | TEXT | RODATA | DATA | BSS | [SYMS] | [PRELOADED MODULES] | L4 ->
 * +------+--------+------+-----+--------+---------------------+----------
 *                             (1)      (2)                   (3)
 *
 * --------------+-----+-----+----+-------------+
 * -> PROC0 STK -> L3 -> L2 -> L1 | ISA I/O MEM |
 * --------------+-----+-----+----+-------------+
 *                               (4)
 *
 * PROC0 STK is obviously not linked as a page level. It just happens to be
 * caught between L4 and L3.
 *
 * (PROC0 STK + L4 + L3 + L2 + L1) is later referred to as BOOTSTRAP TABLES.
 *
 * ISA I/O MEM has no physical page allocated here, just virtual addresses.
 *
 * Important note: the kernel segments are properly 4k-aligned
 * (see kern.ldscript), so there's no need to enforce alignment.
 */

	/* Find end of kernel image; brings us on (1). */
	movl	$RELOC(__kernel_end),%edi

#if (NKSYMS || defined(DDB) || defined(MODULAR)) && !defined(makeoptions_COPY_SYMTAB)
	/* Save the symbols (if loaded); brinds us on (2). */
	movl	RELOC(esym),%eax
	testl	%eax,%eax
	jz	1f
	subl	$KERNBASE_LO,%eax	/* XXX */
	movl	%eax,%edi
1:
#endif
	/* Skip over any modules/blobs; brings us on (3). */
	movl	RELOC(eblob),%eax
	testl	%eax,%eax
	jz	1f
	subl	$KERNBASE_LO,%eax	/* XXX */
	movl	%eax,%edi
1:

	/* We are on (3). Align up for BOOTSTRAP TABLES. */
	movl	%edi,%esi
	addl	$PGOFSET,%esi
	andl	$~PGOFSET,%esi

	/* We are on the BOOTSTRAP TABLES. Save L4's physical address. */
	movl	$RELOC(PDPpaddr),%ebp
	movl	%esi,(%ebp)
	movl	$0,4(%ebp)

	/* Now, zero out the BOOTSTRAP TABLES (before filling them in). */
	movl	%esi,%edi
	xorl	%eax,%eax
	cld
	movl	$TABLESIZE,%ecx
	shrl	$2,%ecx
	rep
	stosl				/* copy eax -> edi */

/*
 * Build the page tables and levels. We go from L1 to L4, and link the levels
 * together. Note: RELOC computes &addr - KERNBASE in 32 bits; the value can't
 * be > 4G, or we can't deal with it anyway, since we are in 32bit mode.
 */
	/*
	 * Build L1.
	 */
	leal	(PROC0_PTP1_OFF)(%esi),%ebx

	/* Skip the area below the kernel text. */
	movl	$(KERNTEXTOFF_LO - KERNBASE_LO),%ecx
	shrl	$PGSHIFT,%ecx
	fillkpt_blank

	/* Map the kernel text RX. */
	movl	$(KERNTEXTOFF_LO - KERNBASE_LO),%eax	/* start of TEXT */
	movl	$RELOC(__rodata_start),%ecx
	subl	%eax,%ecx
	shrl	$PGSHIFT,%ecx
	orl	$(PTE_P),%eax
	fillkpt

	/* Map the kernel rodata R. */
	movl	$RELOC(__rodata_start),%eax
	movl	$RELOC(__data_start),%ecx
	subl	%eax,%ecx
	shrl	$PGSHIFT,%ecx
	orl	$(PTE_P),%eax
	fillkpt_nox

	/* Map the kernel data+bss RW. */
	movl	$RELOC(__data_start),%eax
	movl	$RELOC(__kernel_end),%ecx
	subl	%eax,%ecx
	shrl	$PGSHIFT,%ecx
	orl	$(PTE_P|PTE_W),%eax
	fillkpt_nox

	/* Map [SYMS]+[PRELOADED MODULES] RW. */
	movl	$RELOC(__kernel_end),%eax
	movl	%esi,%ecx		/* start of BOOTSTRAP TABLES */
	subl	%eax,%ecx
	shrl	$PGSHIFT,%ecx
	orl	$(PTE_P|PTE_W),%eax
	fillkpt_nox

	/* Map the BOOTSTRAP TABLES RW. */
	movl	%esi,%eax		/* start of BOOTSTRAP TABLES */
	movl	$TABLESIZE,%ecx		/* length of BOOTSTRAP TABLES */
	shrl	$PGSHIFT,%ecx
	orl	$(PTE_P|PTE_W),%eax
	fillkpt_nox

	/* We are on (4). Map ISA I/O MEM RW. */
	movl	$IOM_BEGIN,%eax
	movl	$IOM_SIZE,%ecx	/* size of ISA I/O MEM */
	shrl	$PGSHIFT,%ecx
	orl	$(PTE_P|PTE_W/*|PTE_PCD*/),%eax
	fillkpt_nox

	/*
	 * Build L2. Linked to L1.
	 */
	leal	(PROC0_PTP2_OFF)(%esi),%ebx
	leal	(PROC0_PTP1_OFF)(%esi),%eax
	orl	$(PTE_P|PTE_W),%eax
	movl	$(NKL2_KIMG_ENTRIES+1),%ecx
	fillkpt

#if L2_SLOT_KERNBASE > 0
	/* If needed, set up level 2 entries for actual kernel mapping */
	leal	(PROC0_PTP2_OFF + L2_SLOT_KERNBASE * PDE_SIZE)(%esi),%ebx
	leal	(PROC0_PTP1_OFF)(%esi),%eax
	orl	$(PTE_P|PTE_W),%eax
	movl	$(NKL2_KIMG_ENTRIES+1),%ecx
	fillkpt
#endif

	/*
	 * Build L3. Linked to L2.
	 */
	leal	(PROC0_PTP3_OFF)(%esi),%ebx
	leal	(PROC0_PTP2_OFF)(%esi),%eax
	orl	$(PTE_P|PTE_W),%eax
	movl	$NKL3_KIMG_ENTRIES,%ecx
	fillkpt

#if L3_SLOT_KERNBASE > 0
	/* If needed, set up level 3 entries for actual kernel mapping */
	leal	(PROC0_PTP3_OFF + L3_SLOT_KERNBASE * PDE_SIZE)(%esi),%ebx
	leal	(PROC0_PTP2_OFF)(%esi),%eax
	orl	$(PTE_P|PTE_W),%eax
	movl	$NKL3_KIMG_ENTRIES,%ecx
	fillkpt
#endif

	/*
	 * Build L4 for identity mapping. Linked to L3.
	 */
	leal	(PROC0_PML4_OFF)(%esi),%ebx
	leal	(PROC0_PTP3_OFF)(%esi),%eax
	orl	$(PTE_P|PTE_W),%eax
	movl	$NKL4_KIMG_ENTRIES,%ecx
	fillkpt

	/* Set up L4 entries for actual kernel mapping */
	leal	(PROC0_PML4_OFF + L4_SLOT_KERNBASE * PDE_SIZE)(%esi),%ebx
	leal	(PROC0_PTP3_OFF)(%esi),%eax
	orl	$(PTE_P|PTE_W),%eax
	movl	$NKL4_KIMG_ENTRIES,%ecx
	fillkpt

	/*
	 * Startup checklist:
	 * 1. Enable PAE (and SSE while here).
	 */
	movl	%cr4,%eax
	orl	$(CR4_PAE|CR4_OSFXSR|CR4_OSXMMEXCPT),%eax
	movl	%eax,%cr4

	/*
	 * 2. Set Long Mode Enable in EFER. Also enable the syscall extensions,
	 *    and NOX if available.
	 */
	movl	$MSR_EFER,%ecx
	rdmsr
	xorl	%eax,%eax	/* XXX */
	orl	$(EFER_LME|EFER_SCE),%eax
	movl	RELOC(nox_flag),%ebx
	cmpl	$0,%ebx
	je 	.Lskip_NOX
	orl	$(EFER_NXE),%eax
.Lskip_NOX:
	wrmsr

	/*
	 * 3. Load %cr3 with pointer to PML4.
	 */
	movl	%esi,%eax
	movl	%eax,%cr3

	/*
	 * 4. Enable paging and the rest of it.
	 */
	movl	%cr0,%eax
	orl	$(CR0_PE|CR0_PG|CR0_NE|CR0_TS|CR0_MP|CR0_WP|CR0_AM),%eax
	movl	%eax,%cr0
	jmp	compat
compat:

	movl	$RELOC(tmpstk),%esp
	/*
	 * 5. Not quite done yet, we're now in a compatibility segment, in
	 *    legacy mode. We must jump to a long mode segment. Need to set up
	 *    a temporary GDT with a long mode segment in it to do that.
	 */
	movl	$RELOC(gdt64_lo),%eax
	lgdt	(%eax)
	movl	$RELOC(farjmp64),%eax
	ljmp	*(%eax)

	.code64
longmode:
	/*
	 * 6. Finally, we're in long mode. However, we're still in the identity
	 *    mapped area (could not jump out of that earlier because it would
	 *    have been a > 32bit jump). We can do that now, so here we go.
	 */
	movabsq	$longmode_hi,%rax
	jmp	*%rax

longmode_hi:

	/*
	 * We left the identity mapped area. Base address of
	 * the temporary gdt64 should now be in high memory.
	 */
	movq	$RELOC(gdt64_hi),%rax
	lgdt	(%rax)

	/*
	 * We have arrived. There's no need anymore for the identity mapping in
	 * low memory, remove it.
	 */
	movq	$KERNBASE,%r8

#if L2_SLOT_KERNBASE > 0
	movq	$(NKL2_KIMG_ENTRIES+1),%rcx
	leaq	(PROC0_PTP2_OFF)(%rsi),%rbx	/* old, phys address */
	addq	%r8,%rbx			/* new, virt address */
	killkpt
#endif

#if L3_SLOT_KERNBASE > 0
	movq	$NKL3_KIMG_ENTRIES,%rcx
	leaq	(PROC0_PTP3_OFF)(%rsi),%rbx	/* old, phys address */
	addq	%r8,%rbx			/* new, virt address */
	killkpt
#endif

	movq	$NKL4_KIMG_ENTRIES,%rcx
	leaq	(PROC0_PML4_OFF)(%rsi),%rbx	/* old, phys address of PML4 */
	addq	%r8,%rbx			/* new, virt address of PML4 */
	killkpt

	/* Relocate atdevbase. */
	movq	$(TABLESIZE+KERNBASE),%rdx
	addq	%rsi,%rdx
	movq	%rdx,_C_LABEL(atdevbase)(%rip)

	/* Set up bootstrap stack. */
	leaq	(PROC0_STK_OFF)(%rsi),%rax
	addq	%r8,%rax
	movq	%rax,_C_LABEL(lwp0uarea)(%rip)
	leaq	(USPACE-FRAMESIZE)(%rax),%rsp
	xorq	%rbp,%rbp			/* mark end of frames */

	xorw	%ax,%ax
	movw	%ax,%gs
	movw	%ax,%fs

	/* The first physical page available. */
	leaq	(TABLESIZE)(%rsi),%rdi

#else	/* XENPV */
	/* First, reset the PSL. */
	pushq	$2
	popfq

	cld

	/*
	 * Xen info:
	 * - %rsi -> start_info struct
	 * - %rsp -> stack, *theoretically* the last used page by Xen bootstrap
	 */
	movq	%rsi,%rbx

	/* Clear BSS. */
	xorq	%rax,%rax
	movq	$_C_LABEL(__bss_start),%rdi
	movq	$_C_LABEL(_end),%rcx
	subq	%rdi,%rcx
	rep
	stosb

	/* Copy start_info to a safe place. */
	movq	%rbx,%rsi
	movq	$_C_LABEL(start_info_union),%rdi
	movq	$(PAGE_SIZE / 8),%rcx
	rep
	movsq

	/*
	 * Memory layout at start of the day:
	 * - Kernel image
	 * - Page frames list
	 * - start_info struct. we copied it, so it can be recycled.
	 * - xenstore
	 * - console
	 * - Xen bootstrap page tables
	 * - kernel stack. provided by Xen
	 * - guaranteed 512kB padding
	 *
	 * As we want to rebuild our page tables and place our stack
	 * in proc0 struct, all data starting from after console can be
	 * discarded after we've done a little setup.
	 */

	/*
	 * We want our own page tables, and will rebuild them. We will reclaim
	 * the Xen space later, INCLUDING the stack. So we need to switch to a
	 * temporary one now.
	 */
	movq	$tmpstk,%rax
	subq	$8,%rax
	movq	%rax,%rsp

	xorl	%eax,%eax
	cpuid
	movl	%eax,_C_LABEL(cpuid_level)

	movl	$VM_GUEST_XENPV, _C_LABEL(vm_guest)

	movq	$cpu_info_primary,%rdi
	movq	%rdi,CPU_INFO_SELF(%rdi) /* ci->ci_self = ci */
	movq	$1,%rsi
	call	cpu_init_msrs	/* cpu_init_msrs(ci, true); */

	call	xen_locore

	/*
	 * The first VA available is returned by xen_locore in %rax. We
	 * use it as the UAREA, and set up the stack here.
	 */
	movq	%rax,%rsi
	movq	%rsi,_C_LABEL(lwp0uarea)(%rip)
	leaq	(USPACE-FRAMESIZE)(%rsi),%rsp
	xorq	%rbp,%rbp

	/* Clear segment registers. */
	xorw	%ax,%ax
	movw	%ax,%gs
	movw	%ax,%fs

	/* Set first_avail after the DUMMY PAGE (see xen_locore). */
	movq	%rsi,%rdi
	addq	$(USPACE+PAGE_SIZE),%rdi
	subq	$KERNBASE,%rdi	/* init_x86_64 wants a physical address */
#endif	/* XENPV */

	pushq	%rdi
	call	_C_LABEL(init_bootspace)
#ifdef KASAN
	movq	_C_LABEL(lwp0uarea)(%rip),%rdi
	call	_C_LABEL(kasan_early_init)
#endif
	/* <-- DO NOT INSERT C CALLS BEFORE THIS POINT --> */
#if defined(XEN) && !defined(XENPV)
	call	_C_LABEL(init_xen_early)
#endif
	call	_C_LABEL(init_slotspace)
	popq	%rdi
	call	_C_LABEL(init_x86_64)
	call 	_C_LABEL(main)
END(start)

#if defined(XEN)
# if !defined(XENPV)
/* entry point for Xen PVH */
	.code32
ENTRY(start_pvh)
	/* Xen doesn't start us with a valid gdt */
	movl    $RELOC(gdtdesc32), %eax
	lgdt    (%eax)
	jmp     $GSEL(GCODE_SEL, SEL_KPL), $RELOC(.Lreload_cs)

.Lreload_cs:
	movw    $GSEL(GDATA_SEL, SEL_KPL), %ax          
	movw    %ax, %ds
	movw    %ax, %es
	movw    %ax, %ss   

	/* we need a valid stack */
	movl	$RELOC(tmpstk),%esp

	/* clear BSS */
        xorl    %eax,%eax
	movl    $RELOC(__bss_start),%edi
	movl    $RELOC(_end),%ecx
	subl    %edi,%ecx
	rep
	stosb

	/*
	 * Here, we have 2 cases :
	 *
	 *  1) We have been started by Xen
	 *  2) We have been started by another VMM (Qemu, Firecracker, ...)
	 *
	 * The main difference is that, when we are started by Xen,
	 * %ebx (addr of the hvm_start_info structure) is pointing to a
	 * location that will be mapped correctly later.
	 *
	 * In the second case, we have to copy this structure (and all
	 * the information contained in it) to a location that will be
	 * mapped later : __kernel_end
	 *
	 * To distinguish between the 2 cases, we'll use the 'cpuid' instruction
	 */
	push %ebx
	xorl %eax, %eax
	cpuid
	cmpl $0x1, %eax		/* Check if we can call CPUID with eax=1 */
	jb .start_genpvh
	xorl %eax, %eax
	inc %eax
	cpuid
	shr $31, %ecx
	testb $1, %cl		/* Check if bit 31 of ECX (hypervisor) is set */
	jz .start_genpvh
	xorl %eax, %eax
	inc %eax
	shl $30, %eax
	cpuid			/* Calling cpuid with eax=0x40000000 */
	cmp $XEN_CPUID_SIGNATURE_EBX, %ebx	/* "VneX" */
	je .start_xen32

	/* We have been started by a VMM that is *not* Xen */

.start_genpvh:

	/* announce ourself */
	movl $VM_GUEST_GENPVH, RELOC(vm_guest)

	pop %ebx
	movl $RELOC(__kernel_end), %eax
	movl %eax, %ecx
	addl $KERNBASE_LO,%ecx
	movl $RELOC(esym),%ebp
	movl %ecx,(%ebp)
	movl $KERNBASE_HI,4(%ebp)

	jmp .copy_hvm_info

.start_xen32:
	movl $VM_GUEST_XENPVH, RELOC(vm_guest)
	/*
	 * Read the size of the symbol table, sanity-check and compute the end
	 * We have:
	 * |   kernel   |
	 * -------------- kernel_end
	 *     alignment
	 * -------------- bsd_symtab
	 * | size (int) |
	 * | elf_header |
	 * 
	 */
	movl $RELOC(__kernel_end), %ebp
	addl $3, %ebp
	andl $~3, %ebp
	movl 0(%ebp), %eax /* read size */
	testl $~0x00ffffff, %eax /* more than 16MB ? */
	jnz .bad_esym
	addl %ebp, %eax /* compute esym */
	/* check if start_info is within symbol table */
	movl 0(%esp), %ebx
	cmp %ebp, %ebx
	jb .save_esym /* %ebx < __kernel_end */
	cmp %eax, %ebx
	jae .save_esym /* %ebx >= esym */

.bad_esym:
	movl $RELOC(__kernel_end), %eax
.save_esym:
	movl %eax, %ebx
	addl $KERNBASE_LO,%ebx
	movl $RELOC(esym),%ebp
	movl %ebx,(%ebp)
	movl $KERNBASE_HI,4(%ebp)
	/* advance to next page boundary, this will be our hvm_start_info */
	addl $PGOFSET,%eax
	andl $~PGOFSET,%eax
	pop %ebx
	
.copy_hvm_info:
 	/*
	 * %ebx points to physical address provided by Xen
	 * %eax points to where we want it to be copied to
	 */
	/* check if %ebx and %eax are in the same page */
	movl %ebx, %esi
	addl $PGOFSET,%esi
	andl $~PGOFSET,%esi
	cmp %esi, %eax
	je .same_hvm_info

	/* First, copy the hvm_start_info structure to %eax */
	movl %ebx, %esi
	movl %eax, %edi
	movl $HVM_START_INFO_SIZE, %ecx
	shrl $2, %ecx
	rep movsl

	/* Copy cmdline_paddr after hvm_start_info */
	movl CMDLINE_PADDR(%ebx), %esi
	movl %edi, CMDLINE_PADDR(%eax)	/* Set new cmdline_paddr in hvm_start_info */
.cmdline_copy:
	movb (%esi), %cl
	movsb
	cmp $0, %cl
	jne .cmdline_copy

	/* Copy memmap_paddr after cmdline (only if hvm_start_info->version != 0) */
	xorl %ecx, %ecx
	cmpl START_INFO_VERSION(%ebx), %ecx
	je .save_hvm_info
	pushl %eax
	movl MMAP_PADDR(%ebx), %esi
	movl %edi, MMAP_PADDR(%eax)	/* Set new memmap_paddr in hvm_start_info */
	movl MMAP_ENTRIES(%ebx), %eax	/* Get memmap_entries */
	movl $MMAP_ENTRY_SIZE, %ebx
	mull %ebx			/* eax * ebx => edx:eax */
	movl %eax, %ecx
	shrl $2, %ecx
	rep movsl
	popl %eax

.save_hvm_info:
	/*
	 * %eax points to the start of hvm_start_info
	 * %edi points to the end
	 */
	addl 	$KERNBASE_LO,%eax
	movl	$RELOC(hvm_start_info),%ebp
	movl	%eax,(%ebp)
	movl	$KERNBASE_HI,4(%ebp)

	/* round end to next page boundary */
	addl	$PGOFSET,%edi
	andl	$~PGOFSET,%edi

	/* get a page for HYPERVISOR_shared_info */
	/* this is only needed if we are running on Xen */
	cmpl	$VM_GUEST_XENPVH, RELOC(vm_guest)
	jne	.save_eblob
	movl	$RELOC(HYPERVISOR_shared_info_pa),%ebp
	movl	%edi,(%ebp)
	movl	$0,4(%ebp)
	addl	$PAGE_SIZE, %edi
.save_eblob:
	addl	$KERNBASE_LO,%edi
	movl	$RELOC(eblob),%ebp
	movl	%edi,(%ebp)
	movl	$KERNBASE_HI,4(%ebp)

	jmp .Lbiosbasemem_finished

.same_hvm_info:
	/* just use the provided %ebx */
	/* XXX assume hvm_start_info+dependant structure fits in a single page */
	movl %ebx, %eax
	movl %ebx, %edi
	addl	$PAGE_SIZE, %edi
	jmp .save_hvm_info
END(start_pvh)
	.code64
# endif /* !XENPV */
/* space for the hypercall call page */
#define HYPERCALL_PAGE_OFFSET 0x1000
.align HYPERCALL_PAGE_OFFSET
ENTRY(hypercall_page) /* Returns -1, on HYPERVISOR_xen_version() */
.skip	(__HYPERVISOR_xen_version*32), 0x90
	movq	$-1, %rax
	retq
.align HYPERCALL_PAGE_OFFSET, 0x90
END(hypercall_page)
#endif /* XEN */

/*
 * int setjmp(label_t *)
 *
 * Used primarily by DDB.
 */
ENTRY(setjmp)
	/*
	 * Only save registers that must be preserved across function
	 * calls according to the ABI (%rbx, %rsp, %rbp, %r12-%r15)
	 * and %rip.
	 */
	movq	%rdi,%rax
	movq	%rbx,(%rax)
	movq	%rsp,8(%rax)
	movq	%rbp,16(%rax)
	movq	%r12,24(%rax)
	movq	%r13,32(%rax)
	movq	%r14,40(%rax)
	movq	%r15,48(%rax)
	movq	(%rsp),%rdx
	movq	%rdx,56(%rax)
	xorl	%eax,%eax
	ret
END(setjmp)

/*
 * int longjmp(label_t *)
 *
 * Used primarily by DDB.
 */
ENTRY(longjmp)
	movq	%rdi,%rax
	movq	(%rax),%rbx
	movq	8(%rax),%rsp
	movq	16(%rax),%rbp
	movq	24(%rax),%r12
	movq	32(%rax),%r13
	movq	40(%rax),%r14
	movq	48(%rax),%r15
	movq	56(%rax),%rdx
	movq	%rdx,(%rsp)
	movl	$1,%eax
	ret
END(longjmp)

/*
 * void dumpsys(void)
 *
 * Mimic cpu_switchto() for postmortem debugging.
 */
ENTRY(dumpsys)
	/* Build a fake switch frame. */
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15

	/* Save a context. */
	movq	$dumppcb, %rax
	movq	%rsp, PCB_RSP(%rax)
	movq	%rbp, PCB_RBP(%rax)

	call	_C_LABEL(dodumpsys)

	addq	$(5*8), %rsp	/* sizeof(switchframe) - sizeof(%rip) */
	ret
END(dumpsys)

/*
 * struct lwp *cpu_switchto(struct lwp *oldlwp, struct lwp *newlwp,
 *     bool returning)
 *
 *	1. save context of oldlwp.
 *	2. restore context of newlwp.
 *
 * Note that the stack frame layout is known to "struct switchframe" in
 * <machine/frame.h> and to the code in cpu_lwp_fork() which initializes
 * it for a new lwp.
 */
ENTRY(cpu_switchto)
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15

	movq	%rdi,%r13	/* oldlwp */
	movq	%rsi,%r12	/* newlwp */

	/* Save old context. */
	movq	L_PCB(%r13),%rax
	movq	%rsp,PCB_RSP(%rax)
	movq	%rbp,PCB_RBP(%rax)

	/* Switch to newlwp's stack. */
	movq	L_PCB(%r12),%r14
	movq	PCB_RSP(%r14),%rsp
	movq	PCB_RBP(%r14),%rbp

	/*
	 * Set curlwp.  This must be globally visible in order to permit
	 * non-interlocked mutex release.
	 */
	movq	%r12,%rcx
	xchgq	%rcx,CPUVAR(CURLWP)

#ifdef XENPV
	/* if we are there, we're obviously not in user context.
	 * reset ci_xen_clockf_* in case the splx() at the end of mi_switch()
	 * triggers a deffered call do xen_timer_handler()
	 */
	movb    $0, CPUVAR(XEN_CLOCKF_USERMODE)
	movq    $_C_LABEL(cpu_switchto), CPUVAR(XEN_CLOCKF_PC)
#endif


	/* Skip the rest if returning to a pinned LWP. */
	testb	%dl,%dl		/* returning = true ? */
	jnz	.Lswitch_return

#ifdef SVS
	movb	_C_LABEL(svs_enabled),%dl
	testb	%dl,%dl
	jz	.Lskip_svs
	callq	_C_LABEL(svs_lwp_switch)
.Lskip_svs:
#endif

#ifndef XENPV
	movq	%r13,%rdi
	movq	%r12,%rsi
	callq	_C_LABEL(speculation_barrier)
#endif

	/* Switch ring0 stack */
#ifdef SVS
	movb	_C_LABEL(svs_enabled),%al
	testb	%al,%al
	jz	.Lno_svs_switch

	movq	CPUVAR(RSP0),%rax
	movq	CPUVAR(TSS),%rdi
	movq	%rax,TSS_RSP0(%rdi)
	jmp	.Lring0_switched

.Lno_svs_switch:
#endif

#if !defined(XENPV)
	movq	PCB_RSP0(%r14),%rax
	movq	CPUVAR(TSS),%rdi
	movq	%rax,TSS_RSP0(%rdi)
#else
	movq	%r14,%rdi
	callq	_C_LABEL(x86_64_switch_context)
#endif
.Lring0_switched:

	/* Switch the dbregs. */
	movq	%r13,%rdi
	movq	%r12,%rsi
	callq	_C_LABEL(x86_dbregs_switch)

	/* Switch the FPU. */
	movq	%r13,%rdi
	movq	%r12,%rsi
	callq	_C_LABEL(fpu_switch)

	/* Don't bother with the rest if switching to a system process. */
	testl	$LW_SYSTEM,L_FLAG(%r12)
	jnz	.Lswitch_return

	/* Is this process using RAS (restartable atomic sequences)? */
	movq	L_PROC(%r12),%rdi
	cmpq	$0,P_RASLIST(%rdi)
	je	.Lno_RAS

	/* Handle restartable atomic sequences (RAS). */
	movq	L_MD_REGS(%r12),%rbx
	movq	TF_RIP(%rbx),%rsi
	call	_C_LABEL(ras_lookup)
	cmpq	$-1,%rax
	je	.Lno_RAS
	movq	%rax,TF_RIP(%rbx)
.Lno_RAS:

#ifndef XENPV
	/* Raise the IPL to IPL_HIGH. Dropping the priority is deferred until
	 * mi_switch(), when cpu_switchto() returns. XXX Still needed? */
	movb	$IPL_HIGH,CPUVAR(ILEVEL)

	/* The 32bit LWPs are handled differently. */
	testl	$PCB_COMPAT32,PCB_FLAGS(%r14)
	jnz	.Llwp_32bit

.Llwp_64bit:
	/* Set default 64bit values in %ds, %es, %fs and %gs. */
	movq	$GSEL(GUDATA_SEL, SEL_UPL),%rax
	movw	%ax,%ds
	movw	%ax,%es
	xorq	%rax,%rax
	movw	%ax,%fs
	CLI(cx)
	SWAPGS
	movw	%ax,%gs
	SWAPGS
	STI(cx)

	/* Zero out GDT descriptors. */
	movq	CPUVAR(GDT),%rcx
	movq	%rax,(GUFS_SEL*8)(%rcx)
	movq	%rax,(GUGS_SEL*8)(%rcx)

	/* Reload 64-bit %fs/%gs MSRs. */
	movl	$MSR_FSBASE,%ecx
	movl	PCB_FS(%r14),%eax
	movl	4+PCB_FS(%r14),%edx
	wrmsr
	movl	$MSR_KERNELGSBASE,%ecx
	movl	PCB_GS(%r14),%eax
	movl	4+PCB_GS(%r14),%edx
	wrmsr

	jmp	.Lswitch_return

.Llwp_32bit:
	/* Reload %fs/%gs GDT descriptors. */
	movq	CPUVAR(GDT),%rcx
	movq	PCB_FS(%r14),%rax
	movq	%rax,(GUFS_SEL*8)(%rcx)
	movq	PCB_GS(%r14),%rax
	movq	%rax,(GUGS_SEL*8)(%rcx)

	/* Set default 32bit values in %ds, %es, %fs and %gs. */
	movq	L_MD_REGS(%r12),%rbx
	movq	$GSEL(GUDATA32_SEL, SEL_UPL),%rax
	movw	%ax,%ds
	movw	%ax,%es
	movw	%ax,%fs
	CLI(ax)
	SWAPGS
	movw	%ax,%gs
	SWAPGS
	STI(ax)
#else
	movq	%r12,%rdi
	callq	_C_LABEL(x86_64_tls_switch)
#endif

.Lswitch_return:
	/* Return to the new LWP, returning 'oldlwp' in %rax. */
	KMSAN_INIT_RET(8)
	movq	%r13,%rax
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbx
	ret
END(cpu_switchto)

/*
 * void savectx(struct pcb *pcb);
 *
 * Update pcb, saving current processor state.
 */
ENTRY(savectx)
	/* Save stack pointers. */
	movq	%rsp,PCB_RSP(%rdi)
	movq	%rbp,PCB_RBP(%rdi)
	ret
END(savectx)

/*
 * Syscall handler.
 */
ENTRY(handle_syscall)
	STI(si)

	movq	CPUVAR(CURLWP),%r14
	incq	CPUVAR(NSYSCALL)	/* count it atomically */
	movq	%rsp,L_MD_REGS(%r14)	/* save pointer to frame */
	movq	L_PROC(%r14),%r15
	andl	$~MDL_IRET,L_MD_FLAGS(%r14)   /* Allow sysret return */
	movq	%rsp,%rdi		/* Pass frame as arg0 */
	call	*P_MD_SYSCALL(%r15)
.Lsyscall_checkast:
	/*
	 * Disable interrupts to avoid new ASTs (etc) being added and
	 * to ensure we don't take an interrupt with some of the user
	 * registers loaded.
	 */
	CLI2(si, rax)
	/* Check for ASTs on exit to user mode. */
	movl	L_MD_ASTPENDING(%r14),%eax
	orl	CPUVAR(WANT_PMAPLOAD),%eax
	jnz	9f

#ifdef DIAGNOSTIC
	cmpb	$IPL_NONE,CPUVAR(ILEVEL)
	jne	.Lspl_error
#endif

	HANDLE_DEFERRED_FPU

	/*
	 * Decide if we need to take a slow path. That's the case when we
	 * want to reload %cs and %ss on a 64bit LWP (MDL_IRET set), or when
	 * we're returning to a 32bit LWP (MDL_COMPAT32 set).
	 *
	 * In either case, we jump into intrfastexit and return to userland
	 * with the iret instruction.
	 */
	testl	$(MDL_IRET|MDL_COMPAT32),L_MD_FLAGS(%r14)
	jnz	intrfastexit

	jmp	syscall_sysret

#ifdef DIAGNOSTIC
.Lspl_error:
	movabsq	$4f,%rdi
	movzbl	CPUVAR(ILEVEL),%esi
	call	_C_LABEL(panic)
4:	.asciz	"spl not lowered on syscall, ilevel=%x"
#endif

/* AST pending or pmap load needed */
9:
	cmpl	$0,CPUVAR(WANT_PMAPLOAD)
	jz	10f
	STI(si)
	call	_C_LABEL(do_pmap_load)
	jmp	.Lsyscall_checkast	/* re-check ASTs */
10:
	CLEAR_ASTPENDING(%r14)
	STI(si)
	/* Pushed T_ASTFLT into tf_trapno on entry. */
	movq	%rsp,%rdi
	KMSAN_INIT_ARG(8)
	call	_C_LABEL(trap)
	jmp	.Lsyscall_checkast	/* re-check ASTs */
END(handle_syscall)

/*
 * void lwp_trampoline(void);
 *
 * This is a trampoline function pushed run by newly created LWPs
 * in order to do additional setup in their context.
 */
ENTRY(lwp_trampoline)
	movq	%rbp,%rsi
	movq	%rbp,%r14	/* for .Lsyscall_checkast */
	movq	%rax,%rdi
	xorq	%rbp,%rbp
	KMSAN_INIT_ARG(16)
	call	_C_LABEL(lwp_startup)
	movq	%r13,%rdi
	KMSAN_INIT_ARG(8)
	call	*%r12
	jmp	.Lsyscall_checkast
END(lwp_trampoline)

/*
 * Entry points of the 'syscall' instruction, 64bit and 32bit mode.
 */

#define SP(x)	(x)-(TF_SS+8)(%rax)

.macro	SYSCALL_ENTRY	name,is_svs
IDTVEC(\name)
#ifndef XENPV
	/*
	 * The user %rip is in %rcx and the user %rflags in %r11. The kernel %cs
	 * and %ss are loaded, but nothing else is.
	 *
	 * The 'swapgs' instruction gives us access to cpu-specific memory where
	 * we can save a user register and then read the LWP's kernel stack
	 * pointer.
	 *
	 * This code doesn't seem to set %ds, this may not matter since it is
	 * ignored in 64bit mode, OTOH the syscall instruction sets %ss and that
	 * is ignored as well.
	 */
	swapgs

	/* Get the LWP's kernel stack pointer in %rax */
	.if	\is_svs
		movabs	%rax,SVS_UTLS+UTLS_SCRATCH
		movabs	SVS_UTLS+UTLS_RSP0,%rax
	.else
		movq	%rax,CPUVAR(SCRATCH)
		movq	CPUVAR(CURLWP),%rax
		movq	L_PCB(%rax),%rax
		movq	PCB_RSP0(%rax),%rax
	.endif

	/* Make stack look like an 'int nn' frame */
	movq	$(LSEL(LUDATA_SEL, SEL_UPL)),SP(TF_SS)	/* user %ss */
	movq	%rsp,SP(TF_RSP)				/* user %rsp */
	movq	%r11,SP(TF_RFLAGS)			/* user %rflags */
	movq	$(LSEL(LUCODE_SEL, SEL_UPL)),SP(TF_CS)	/* user %cs */
	movq	%rcx,SP(TF_RIP)				/* user %rip */
	leaq	SP(0),%rsp		/* %rsp now valid after frame */

	/* Restore %rax */
	.if	\is_svs
		movabs	SVS_UTLS+UTLS_SCRATCH,%rax
	.else
		movq	CPUVAR(SCRATCH),%rax
	.endif

	movq	$2,TF_ERR(%rsp)		/* syscall instruction size */
	movq	$T_ASTFLT,TF_TRAPNO(%rsp)
#else
	/*
	 * Xen already switched to kernel stack.
	 * But it didn't disable events
	 */
	pushq	%rsi
	pushq	%rdi
	CLI2(si, rdi)
	popq	%rdi
	popq	%rsi
	addq	$0x10,%rsp	/* gap to match cs:rip */
	pushq	$2		/* error code */
	pushq	$T_ASTFLT
	subq	$TF_REGSIZE,%rsp
	cld
#endif
	INTR_SAVE_GPRS
	IBRS_ENTER
	movw	$GSEL(GUDATA_SEL, SEL_UPL),TF_DS(%rsp)
	movw	$GSEL(GUDATA_SEL, SEL_UPL),TF_ES(%rsp)
	movw	$0,TF_FS(%rsp)
	movw	$0,TF_GS(%rsp)
	.if	\is_svs
		SVS_ENTER
	.endif
	KMSAN_ENTER
	jmp	handle_syscall
IDTVEC_END(\name)
.endm

SYSCALL_ENTRY	syscall,is_svs=0

	TEXT_USER_BEGIN

#ifdef SVS
SYSCALL_ENTRY	syscall_svs,is_svs=1
#endif

IDTVEC(syscall32)
	sysret		/* go away please */
IDTVEC_END(syscall32)

	TEXT_USER_END

/*
 * osyscall()
 *
 * Trap gate entry for int $80 syscall, also used by sigreturn.
 */
	TEXT_USER_BEGIN
IDTVEC(osyscall)
#ifdef XENPV
	pushq	%rsi
	pushq	%rdi
	CLI2(si, rdi)
	popq	%rdi
	popq	%rsi
	movq (%rsp),%rcx
	movq 8(%rsp),%r11
	addq $0x10,%rsp
#endif
	pushq	$2		/* size of instruction for restart */
	pushq	$T_ASTFLT	/* trap # for doing ASTs */
	INTRENTRY
	jmp	handle_syscall
IDTVEC_END(osyscall)
	TEXT_USER_END

/*
 * Return to userland via 'sysret'.
 */
	TEXT_USER_BEGIN
	_ALIGN_TEXT
LABEL(syscall_sysret)
	KMSAN_LEAVE
	MDS_LEAVE
	SVS_LEAVE
	IBRS_LEAVE
	INTR_RESTORE_GPRS
	SWAPGS
#ifndef XENPV
	movq	TF_RIP(%rsp),%rcx	/* %rip for sysret */
	movq	TF_RFLAGS(%rsp),%r11	/* %flags for sysret */
	movq	TF_RSP(%rsp),%rsp
	sysretq
#else
	addq	$TF_RIP,%rsp
	pushq	$256	/* VGCF_IN_SYSCALL */
	jmp	HYPERVISOR_iret
#endif
END(syscall_sysret)
	TEXT_USER_END

	TEXT_USER_BEGIN

/*
 * In intrfastexit, we advance %rsp at the beginning. We then access the
 * segment registers in the trapframe with TF_BACKW (backwards). See the
 * documentation in amd64_trap.S for an explanation.
 */

#define TF_BACKW(val, reg)	(val - (TF_REGSIZE+16))(reg)

	_ALIGN_TEXT
	.type intrfastexit,@function
LABEL(intrfastexit)
	NOT_XEN(cli;)
	KMSAN_LEAVE

	testb	$SEL_UPL,TF_CS(%rsp)
	jz	.Lkexit

	MDS_LEAVE
	SVS_LEAVE
	IBRS_LEAVE
	INTR_RESTORE_GPRS
	addq	$(TF_REGSIZE+16),%rsp	/* iret frame */
	SWAPGS

	cmpw	$LSEL(LUCODE_SEL, SEL_UPL),TF_BACKW(TF_CS, %rsp)
	je	do_iret
	cmpw	$GSEL(GUCODE_SEL, SEL_UPL),TF_BACKW(TF_CS, %rsp)
	je	do_iret
#ifdef XENPV
	cmpw	$FLAT_RING3_CS64,TF_BACKW(TF_CS, %rsp)
	je	do_iret
#endif

do_mov_es:
	movw	TF_BACKW(TF_ES, %rsp),%es
do_mov_ds:
	movw	TF_BACKW(TF_DS, %rsp),%ds
do_mov_fs:
	movw	TF_BACKW(TF_FS, %rsp),%fs
#ifndef XENPV
do_mov_gs:
	movw	TF_BACKW(TF_GS, %rsp),%gs
#endif

do_iret:
	iretq

.Lkexit:
	INTR_RESTORE_GPRS
	addq	$(TF_REGSIZE+16),%rsp	/* iret frame */
	iretq
END(intrfastexit)

	TEXT_USER_END

	.section .rodata

	/*
	 * Hotpatch templates.
	 */

LABEL(hp_nolock)
	nop
LABEL(hp_nolock_end)

LABEL(hp_retfence)
	lfence
LABEL(hp_retfence_end)

LABEL(hp_clac)
	clac
LABEL(hp_clac_end)

LABEL(hp_stac)
	stac
LABEL(hp_stac_end)

#ifdef SVS
LABEL(svs_enter)
	movabs	SVS_UTLS+UTLS_KPDIRPA,%rax
	movq	%rax,%cr3
	movq	CPUVAR(KRSP0),%rsp
LABEL(svs_enter_end)

LABEL(svs_enter_altstack)
	testb	$SEL_UPL,TF_CS(%rsp)
	jz	1234f
	movabs	SVS_UTLS+UTLS_KPDIRPA,%rax
	movq	%rax,%cr3
1234:
LABEL(svs_enter_altstack_end)

LABEL(svs_enter_nmi)
	movq	%cr3,%rax
	movq	%rax,(FRAMESIZE+1*8)(%rsp)	/* nmistore->scratch */
	movq	(FRAMESIZE+0*8)(%rsp),%rax	/* nmistore->cr3 */
	movq	%rax,%cr3
LABEL(svs_enter_nmi_end)

LABEL(svs_leave)
	movq	CPUVAR(URSP0),%rsp
	movq	CPUVAR(UPDIRPA),%rax
	movq	%rax,%cr3
LABEL(svs_leave_end)

LABEL(svs_leave_altstack)
	testb	$SEL_UPL,TF_CS(%rsp)
	jz	1234f
	movq	CPUVAR(UPDIRPA),%rax
	movq	%rax,%cr3
1234:
LABEL(svs_leave_altstack_end)

LABEL(svs_leave_nmi)
	movq	(FRAMESIZE+1*8)(%rsp),%rax	/* nmistore->scratch */
	movq	%rax,%cr3
LABEL(svs_leave_nmi_end)
#endif

	/* IBRS <- 1 */
LABEL(ibrs_enter)
	movl	$MSR_IA32_SPEC_CTRL,%ecx
	rdmsr
	orl	$IA32_SPEC_CTRL_IBRS,%eax
	wrmsr
LABEL(ibrs_enter_end)

	/* IBRS <- 0 */
LABEL(ibrs_leave)
	movl	$MSR_IA32_SPEC_CTRL,%ecx
	rdmsr
	andl	$~IA32_SPEC_CTRL_IBRS,%eax
	wrmsr
LABEL(ibrs_leave_end)

LABEL(noibrs_enter)
	NOIBRS_ENTER
LABEL(noibrs_enter_end)

LABEL(noibrs_leave)
	NOIBRS_LEAVE
LABEL(noibrs_leave_end)

LABEL(mds_leave)
	pushq	$GSEL(GDATA_SEL, SEL_KPL)
	verw	(%rsp)
	addq	$8,%rsp
LABEL(mds_leave_end)

LABEL(nomds_leave)
	NOMDS_LEAVE
LABEL(nomds_leave_end)

#ifdef SELFRELOC
/*
 * selfreloc(loadddr edi)
 * This is adapted from sys/arch/i386/i386/locore.S
 */
	.code32
ENTRY(selfreloc_start)
	movl	%edi, %ebx		/* loadaddr saved in ebx */
	movl	%edi, %esi				/* src */
	movl	$_RELOC(kernel_text), %edi		/* dest */
	movl	16(%esp),%ecx				/* esym */
	subl	$_RELOC(kernel_text), %ecx		/* size */

#if defined(NO_OVERLAP)
        movl    %ecx, %eax
#else 
        movl    %edi, %eax
        subl    %esi, %eax
        cmpl    %ecx, %eax      /* overlapping? */
        movl    %ecx, %eax
        jb      .Lbackwards
#endif
        /* nope, copy forwards. */
        shrl    $2, %ecx        /* copy by words */
        rep
        movsl
        and     $3, %eax        /* any bytes left? */
        jnz     .Ltrailing
        jmp     .Lcopy_done

.Ltrailing:
        cmp     $2, %eax
        jb      11f
        movw    (%esi), %ax
        movw    %ax, (%edi)
        je      .Lcopy_done
        movb    2(%esi), %al
        movb    %al, 2(%edi)
        jmp     .Lcopy_done
11:     movb    (%esi), %al
        movb    %al, (%edi)
        jmp     .Lcopy_done

#if !defined(NO_OVERLAP)
.Lbackwards:
        addl    %ecx, %edi      /* copy backwards. */
        addl    %ecx, %esi
        and     $3, %eax        /* any fractional bytes? */
        jnz     .Lback_align
.Lback_aligned:
        shrl    $2, %ecx
        subl    $4, %esi
        subl    $4, %edi
        std
        rep
        movsl
        cld
        jmp     .Lcopy_done

.Lback_align:
        sub     %eax, %esi
        sub     %eax, %edi
        cmp     $2, %eax
        jb      11f
        je      12f
        movb    2(%esi), %al
        movb    %al, 2(%edi)
12:     movw    (%esi), %ax
        movw    %ax, (%edi)
        jmp     .Lback_aligned
11:     movb    (%esi), %al
        movb    %al, (%edi)
        jmp     .Lback_aligned
#endif
        /* End of copy kernel */
.Lcopy_done:
	cld			/* LynxOS depends on it */

	/* load current selfreloc_start addesss in $edi */
	movl	%ebx, %edi	/* loadaddr was saved in ebx */
	addl	$(selfreloc_start - kernel_text), %edi

	/* Prepare jump address */
	lea	(selfreloc_start32a - selfreloc_start)(%edi), %eax
	movl	%eax, (selfreloc_start32r - selfreloc_start)(%edi)

	/* Setup GDT */
	lea	(gdt - selfreloc_start)(%edi), %eax
	mov	%eax, (gdtrr - selfreloc_start)(%edi)
	lgdt	(gdtr - selfreloc_start)(%edi)

	/* Jump to set %cs */
	ljmp	*(selfreloc_start32r - selfreloc_start)(%edi)

	.align	4
selfreloc_start32a:
	movl	$0x10, %eax	/* #define DATA_SEGMENT 0x10 */
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movw	%ax, %ss

	/* Disable Paging in CR0 */
	movl	%cr0, %eax
	andl	$(~CR0_PG), %eax
	movl	%eax, %cr0

	/* Disable PAE in CR4 */
	movl	%cr4, %eax
	andl	$(~CR4_PAE), %eax
	movl	%eax, %cr4

	jmp	selfreloc_start32b

	.align	4
selfreloc_start32b:
	xor	%eax, %eax
	movl	$_RELOC(start), %esi
	jmp	*%esi

	.align	16
selfreloc_start32r:
	.long	0
	.long	0x08	/* #define	CODE_SEGMENT	0x08 */
	.align	16
gdt:
	.long	0, 0
	.byte	0xff, 0xff, 0x00, 0x00, 0x00, 0x9f, 0xcf, 0x00
	.byte	0xff, 0xff, 0x00, 0x00, 0x00, 0x93, 0xcf, 0x00
gdtr:
	.word	gdtr - gdt
gdtrr:
	.quad
END(selfreloc_start)
#endif /* SELFRELOC */
