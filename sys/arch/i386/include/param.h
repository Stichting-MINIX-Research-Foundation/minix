/*	$NetBSD: param.h,v 1.77 2012/04/20 22:23:24 rmind Exp $	*/

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
 *	@(#)param.h	5.8 (Berkeley) 6/28/91
 */

#ifndef _I386_PARAM_H_
#define _I386_PARAM_H_

/*
 * Machine dependent constants for Intel 386.
 */

#ifdef _KERNEL
#include <machine/cpu.h>
#endif

#define	_MACHINE	i386
#define	MACHINE		"i386"
#define	_MACHINE_ARCH	i386
#define	MACHINE_ARCH	"i386"
#define	MID_MACHINE	MID_I386

#define ALIGNED_POINTER(p,t)	1

#define	PGSHIFT		12		/* LOG2(NBPG) */
#define	NBPG		(1 << PGSHIFT)	/* bytes/page */
#define	PGOFSET		(NBPG-1)	/* byte offset into page */
#define	NPTEPG		(NBPG/(sizeof (pt_entry_t)))

#ifndef MAXPHYS
#define MAXPHYS                (64 * 1024)     /* max raw I/O transfer size */
#endif

#if defined(_KERNEL_OPT)
#include "opt_kernbase.h"
#endif /* defined(_KERNEL_OPT) */

#ifdef KERNBASE_LOCORE
#error "You should only re-define KERNBASE"
#endif

#define DEV_BSHIFT      9               /* log2(DEV_BSIZE) */
#define DEV_BSIZE       (1 << DEV_BSHIFT)

/*
 * Mach derived conversion macros
 */
#define	x86_round_pdr(x) \
	((((unsigned long)(x)) + (NBPD_L2 - 1)) & ~(NBPD_L2 - 1))
#define	x86_trunc_pdr(x)	((unsigned long)(x) & ~(NBPD_L2 - 1))
#define	x86_btod(x)		((unsigned long)(x) >> L2_SHIFT)
#define	x86_dtob(x)		((unsigned long)(x) << L2_SHIFT)
#define	x86_round_page(x)	((((paddr_t)(x)) + PGOFSET) & ~PGOFSET)
#define	x86_trunc_page(x)	((paddr_t)(x) & ~PGOFSET)
#define	x86_btop(x)		((paddr_t)(x) >> PGSHIFT)
#define	x86_ptob(x)		((paddr_t)(x) << PGSHIFT)

#ifdef __minix
/* Minix expect to find in this file PAGE_* defines. */
#include <machine/vmparam.h>

#define trunc_page(x) x86_trunc_page(x)
#define round_page(x) x86_round_page(x)

#endif

#endif /* _I386_PARAM_H_ */
