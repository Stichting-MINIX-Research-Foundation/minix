/*	$NetBSD: mptramp.S,v 1.27 2014/02/02 22:41:20 dsl Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by RedBack Networks Inc.
 *
 * Author: Bill Sommerfeld
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
 * Copyright (c) 1999 Stefan Grefen
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
 *      This product includes software developed by the NetBSD 
 *      Foundation, Inc. and its contributors.  
 * 4. Neither the name of The NetBSD Foundation nor the names of its 
 *    contributors may be used to endorse or promote products derived  
 *    from this software without specific prior written permission.   
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR AND CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * MP startup ...
 * the stuff from cpu_spinup_trampoline to mp_startup 
 * is copied into the first 640 KB
 *
 * We startup the processors now when the kthreads become ready.
 * The steps are:
 *        1)   Get the processors running kernel-code from a special
 *                  page-table and stack page, do chip identification.
 *        2)   halt the processors waiting for them to be enabled
 *              by a idle-thread 
 */

#include <machine/asm.h>
__KERNEL_RCSID(0, "$NetBSD: mptramp.S,v 1.27 2014/02/02 22:41:20 dsl Exp $");
	
#include "opt_mpbios.h"		/* for MPDEBUG */
		
#include "assym.h"
#include <machine/specialreg.h>
#include <machine/segments.h>
#include <machine/mpbiosvar.h>
#include <machine/i82489reg.h>
#include <machine/gdt.h>

#define GDTE(a,b)               .byte   0xff,0xff,0x0,0x0,0x0,a,b,0x0
#define _RELOC(x)       ((x) - KERNBASE)
#define RELOC(x)        _RELOC(_C_LABEL(x))

#define _TRMP_LABEL(a)  a = . - _C_LABEL(cpu_spinup_trampoline) + MP_TRAMPOLINE

#ifdef MPDEBUG

/*
 * Debug code to stop aux. processors in various stages based on the
 * value in cpu_trace.
 *
 * %edi points at cpu_trace;  cpu_trace[0] is the "hold point";
 * cpu_trace[1] is the point which the CPU has reached.
 * cpu_trace[2] is the last value stored by HALTT.
 */

#define HALT(x)	1: movl (%edi),%ebx;cmpl $ x,%ebx ; jle 1b ; movl $x,4(%edi)
#define HALTT(x,y)	movl y,8(%edi); HALT(x)
#else
#define HALT(x)	/**/
#define HALTT(x,y) /**/
#endif

	.global _C_LABEL(cpu_spinup_trampoline)
	.global _C_LABEL(cpu_spinup_trampoline_end)
	.global _C_LABEL(mp_pdirpa)

	.text
	.align 4,0x0
	.code16
/* XXX ENTRY() */
LABEL(cpu_spinup_trampoline)
	cli
	xorw    %ax,%ax
	movw    %ax, %ds
	movw    %ax, %es
	movw    %ax, %ss
#ifdef __clang__
	lgdt    (gdt_desc)      # load flat descriptor table
#else
	data32 addr32 lgdt    (gdt_desc)      # load flat descriptor table
#endif
	movl    %cr0, %eax      # get cr0
	orl     $CR0_PE, %eax   # enable protected mode
	movl    %eax, %cr0      # doit
	ljmpl    $0x8, $mp_startup

_TRMP_LABEL(mp_startup)
	.code32

	movl    $0x10, %eax     # data segment
	movw    %ax, %ds
	movw    %ax, %ss
	movw    %ax, %es
	movw    %ax, %fs
	movw    %ax, %gs
	
	movl    $ (MP_TRAMPOLINE+PAGE_SIZE-16),%esp  # bootstrap stack end,
						     # with scratch space..
	
#ifdef MPDEBUG
	leal    RELOC(cpu_trace),%edi       
#endif

	HALT(0x1)
	/* First, reset the PSL. */
	pushl   $PSL_MBO
	popfl
	
	movl	RELOC(pmap_largepages),%eax
	orl	%eax,%eax
	jz	1f
	movl	%cr4,%eax
	orl	$CR4_PSE,%eax
	movl	%eax,%cr4

1:

#ifdef PAE /* Enable PAE */
	movl	%cr4,%eax
	or	$CR4_PAE,%eax
	movl	%eax,%cr4
#endif

	movl	RELOC(mp_pdirpa),%ecx
	HALTT(0x5,%ecx)
	
        /* Load base of page directory and enable mapping. */
        movl    %ecx,%cr3               # load ptd addr into mmu
        movl    %cr0,%eax               # get control word
                                        # enable paging & the fpu
        orl     $(CR0_PE|CR0_PG|CR0_NE|CR0_TS|CR0_MP|CR0_WP|CR0_AM),%eax
        movl    %eax,%cr0               # and page NOW!

#ifdef MPDEBUG
	leal    _C_LABEL(cpu_trace),%edi
#endif
	HALT(0x6)

	/*
	 * If EFER_NXE is not enabled, fetching a page with a NX bit set
	 * will raise a #GP. Avoid that by setting the NXE feature now.
	 */
	movl	_C_LABEL(cpu_feature)+2*4,%eax	/* cpu_feature[2] */
	andl	$CPUID_NOX,%eax
	jz	1f
	movl	$MSR_EFER,%ecx
	rdmsr
	orl	$EFER_NXE,%eax	/* enable No-Execute feature */
	wrmsr

/*
 * ok, we are now running with paging enabled and sharing page tables
 * with cpu0.  figure out which processor we really are, what stack we
 * should be on, etc.
 */

	/* Don't touch lapic until BP has done init sequence. */
1:
	movl	_C_LABEL(cpu_starting),%ecx
	pause
	testl	%ecx, %ecx
	jz	1b
	
	HALTT(0x7, %ecx)

	/* %ecx points at our cpu_info structure. */

	movw    $((MAXGDTSIZ*8) - 1), 6(%esp)	# prepare segment descriptor
	movl    CPU_INFO_GDT(%ecx), %eax	# for our real gdt
	movl    %eax, 8(%esp)
	lgdt	6(%esp)
	HALT(0x8)	
	jmp	1f
	nop
1:	
	HALT(0x12)
	movl    $GSEL(GDATA_SEL, SEL_KPL),%eax 	#switch to new segment
	movl    %eax,%ds
	movl    %eax,%es
	movl    %eax,%ss
	HALT(0x13)
	pushl   $GSEL(GCODE_SEL, SEL_KPL)
	pushl	$mp_cont
	HALT(0x14)
	lret
	.align 4,0x0
_TRMP_LABEL(gdt_table)   
	.word   0x0,0x0,0x0,0x0  # null gdte
	 GDTE(0x9f,0xcf)         # Kernel text
	 GDTE(0x93,0xcf)         # Kernel data
_TRMP_LABEL(gdt_desc)	
	.word   0x17             # limit 3 entries
	.long   gdt_table        # base of gdt
	
_C_LABEL(cpu_spinup_trampoline_end):	#end of code copied to MP_TRAMPOLINE
mp_cont:
	HALT(0x15)

	movl	CPU_INFO_IDLELWP(%ecx),%esi
	movl	L_PCB(%esi),%esi
	
	/* %esi now points at our PCB. */
		
	HALTT(0x19, %esi)
	
	movl	PCB_ESP(%esi),%esp
	movl	PCB_EBP(%esi),%ebp
	
	HALT(0x20)	
	/* Switch address space. */
	movl	PCB_CR3(%esi),%eax
	HALTT(0x22, %eax)		
	movl	%eax,%cr3
	HALT(0x25)
	/* load segment registers. */
	movl	$GSEL(GCPU_SEL, SEL_KPL),%eax
	HALTT(0x26,%eax)	
	movl	%eax,%fs		
	xorl	%eax,%eax
	HALTT(0x27,%eax)		
	movl	%eax,%gs
	movl    PCB_CR0(%esi),%eax
	HALTT(0x28,%eax)		
	movl    %eax,%cr0
	HALTT(0x30,%ecx)	
	pushl	%ecx
	call	_C_LABEL(cpu_hatch)
END(cpu_spinup_trampoline)
	
	.data
LABEL(mp_pdirpa)
	.long	0
END(mp_pdirpa)
#ifdef MPDEBUG
LABEL(cpu_trace)
	.long  0x40
	.long  0xff
	.long  0xff		
END(cpu_trace)
#endif
