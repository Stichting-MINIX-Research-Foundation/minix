/*	$NetBSD: vmparam.h,v 1.39 2015/06/20 07:13:25 skrll Exp $	*/

/*
 * Copyright (c) 2001, 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
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

#ifndef _ARM_ARM32_VMPARAM_H_
#define	_ARM_ARM32_VMPARAM_H_

#if defined(_KERNEL) || defined(_KMEMUSER) || defined(__minix)

/*
 * Virtual Memory parameters common to all arm32 platforms.
 */

#include <arm/cpuconf.h>
#include <arm/arm32/pte.h>	/* pt_entry_t */

#define	__USE_TOPDOWN_VM 
#define	USRSTACK	VM_MAXUSER_ADDRESS

/*
 * ARMv4 systems are normaly configured for 256MB KVA only, so restrict
 * the size of the pager map to 4MB.
 */
#ifndef _ARM_ARCH_5
#define PAGER_MAP_DEFAULT_SIZE          (4 * 1024 * 1024)
#endif

/*
 * Note that MAXTSIZ can't be larger than 32M, otherwise the compiler
 * would have to be changed to not generate "bl" instructions.
 */
#define	MAXTSIZ		(128*1024*1024)		/* max text size */
#ifndef	DFLDSIZ
#define	DFLDSIZ		(384*1024*1024)		/* initial data size limit */
#endif
#ifndef	MAXDSIZ
#define	MAXDSIZ		(1536*1024*1024)	/* max data size */
#endif
#ifndef	DFLSSIZ
#define	DFLSSIZ		(4*1024*1024)		/* initial stack size limit */
#endif
#ifndef	MAXSSIZ
#define	MAXSSIZ		(64*1024*1024)		/* max stack size */
#endif

/*
 * While the ARM architecture defines Section mappings, large pages,
 * and small pages, the standard page size is (and will always be) 4K.
 */
#define	PAGE_SHIFT	PGSHIFT
#define	PAGE_SIZE	(1 << PAGE_SHIFT)
#define	PAGE_MASK	(PAGE_SIZE - 1)

/*
 * Mach derived constants
 */
#define	VM_MIN_ADDRESS		((vaddr_t) PAGE_SIZE)
#ifdef ARM_MMU_EXTENDED
#define	VM_MAXUSER_ADDRESS	((vaddr_t) 0x80000000 - PAGE_SIZE)
#else
#define	VM_MAXUSER_ADDRESS	((vaddr_t) KERNEL_BASE - PAGE_SIZE)
#endif
#define	VM_MAX_ADDRESS		VM_MAXUSER_ADDRESS

#define	VM_MIN_KERNEL_ADDRESS	((vaddr_t) KERNEL_BASE)
#define	VM_MAX_KERNEL_ADDRESS	((vaddr_t) -(PAGE_SIZE+1))

#if !defined(__minix)
#ifndef __ASSEMBLER__
/* XXX max. amount of KVM to be used by buffers. */
#ifndef VM_MAX_KERNEL_BUF
extern vaddr_t virtual_avail;
extern vaddr_t virtual_end;

#define	VM_MAX_KERNEL_BUF	\
	((virtual_end - virtual_avail) * 4 / 10)
#endif
#endif /* __ASSEMBLER__ */
#endif /* !defined(__minix) */

#endif /* _KERNEL || _KMEMUSER */

#endif /* _ARM_ARM32_VMPARAM_H_ */
