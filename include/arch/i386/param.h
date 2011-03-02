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
 * 4. Neither the name of the University nor the names of its contributors
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
 *	from: @(#)param.h	5.8 (Berkeley) 6/28/91
 * $FreeBSD$
 */

#include <machine/_align.h>

#ifndef _I386_INCLUDE_PARAM_H_
#define	_I386_INCLUDE_PARAM_H_

/*
 * Machine dependent constants for Intel 386.
 */

/*
 * Round p (pointer or byte index) up to a correctly-aligned value
 * for all data types (int, long, ...).   The result is unsigned int
 * and must be cast to any desired pointer type.
 */
#ifndef _ALIGNBYTES
#define _ALIGNBYTES	(sizeof(int) - 1)
#endif
#ifndef _ALIGN
#define _ALIGN(p)	(((unsigned)(p) + _ALIGNBYTES) & ~_ALIGNBYTES)
#endif

#define ALIGNBYTES	_ALIGNBYTES
#define ALIGN(p)	_ALIGN(p)
/*
 * ALIGNED_POINTER is a boolean macro that checks whether an address
 * is valid to fetch data elements of type t from on this architecture.
 * This does not reflect the optimal alignment, just the possibility
 * (within reasonable limits).
 */
#define	ALIGNED_POINTER(p, t)	1

#define PAGE_SHIFT	12		/* LOG2(PAGE_SIZE) */
#define PAGE_SIZE	(1<<PAGE_SHIFT)	/* bytes/page */
#define PAGE_MASK	(PAGE_SIZE-1)
#define NPTEPG		(PAGE_SIZE/(sizeof (pt_entry_t)))

#ifdef PAE
#define NPGPTD		4
#define PDRSHIFT	21		/* LOG2(NBPDR) */
#define NPGPTD_SHIFT	9
#else
#define NPGPTD		1
#define PDRSHIFT	22		/* LOG2(NBPDR) */
#define NPGPTD_SHIFT	10
#endif

#define NBPTD		(NPGPTD<<PAGE_SHIFT)
#define NPDEPTD		(NBPTD/(sizeof (pd_entry_t)))
#define NPDEPG		(PAGE_SIZE/(sizeof (pd_entry_t)))
#define NBPDR		(1<<PDRSHIFT)	/* bytes/page dir */
#define PDRMASK		(NBPDR-1)

#define	MAXPAGESIZES	2	/* maximum number of supported page sizes */

/*
 * Mach derived conversion macros
 */
#define trunc_page(x)		((x) & ~PAGE_MASK)
#define round_page(x)		(((x) + PAGE_MASK) & ~PAGE_MASK)
#define trunc_4mpage(x)		((x) & ~PDRMASK)
#define round_4mpage(x)		((((x)) + PDRMASK) & ~PDRMASK)

#define atop(x)			((x) >> PAGE_SHIFT)
#define ptoa(x)			((x) << PAGE_SHIFT)

#define i386_btop(x)		((x) >> PAGE_SHIFT)
#define i386_ptob(x)		((x) << PAGE_SHIFT)

#define	pgtok(x)		((x) * (PAGE_SIZE / 1024))

#define trunc_page_ps(va, ps)	((va) & ~(ps - 1))
#define round_page_ps(va, ps)	(((va) + (ps - 1)) & ~(ps - 1))
#define _minix_aligned(a, t)	(trunc_page_ps((u_long)(a), sizeof(t)) == (u_long)(a))

#endif /* !_I386_INCLUDE_PARAM_H_ */
