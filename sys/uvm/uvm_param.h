/*	$NetBSD: uvm_param.h,v 1.31 2012/03/19 00:17:08 uebayasi Exp $	*/

/*
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * The Mach Operating System project at Carnegie-Mellon University.
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
 *	@(#)vm_param.h	8.2 (Berkeley) 1/9/95
 *
 *
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Avadis Tevanian, Jr., Michael Wayne Young
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 *	Machine independent virtual memory parameters.
 */

#ifndef	_VM_PARAM_
#define	_VM_PARAM_

#ifdef _KERNEL_OPT
#include "opt_modular.h"
#include "opt_uvm.h"
#endif
#ifdef _KERNEL
#include <sys/types.h>
#include <machine/vmparam.h>
#include <sys/resourcevar.h>
#endif

#if defined(_KERNEL)

#if defined(PAGE_SIZE)

/*
 * If PAGE_SIZE is defined at this stage, it must be a constant.
 */

#if PAGE_SIZE == 0
#error Invalid PAGE_SIZE definition
#endif

/*
 * If the platform does not need to support a variable PAGE_SIZE,
 * then provide default values for MIN_PAGE_SIZE and MAX_PAGE_SIZE.
 */

#if !defined(MIN_PAGE_SIZE)
#define	MIN_PAGE_SIZE	PAGE_SIZE
#endif /* ! MIN_PAGE_SIZE */

#if !defined(MAX_PAGE_SIZE)
#define	MAX_PAGE_SIZE	PAGE_SIZE
#endif /* ! MAX_PAGE_SIZE */

#else /* ! PAGE_SIZE */

/*
 * PAGE_SIZE is not a constant; MIN_PAGE_SIZE and MAX_PAGE_SIZE must
 * be defined.
 */

#if !defined(MIN_PAGE_SIZE)
#error MIN_PAGE_SIZE not defined
#endif

#if !defined(MAX_PAGE_SIZE)
#error MAX_PAGE_SIZE not defined
#endif

#endif /* PAGE_SIZE */

/*
 * MIN_PAGE_SIZE and MAX_PAGE_SIZE must be constants.
 */

#if MIN_PAGE_SIZE == 0
#error Invalid MIN_PAGE_SIZE definition
#endif

#if MAX_PAGE_SIZE == 0
#error Invalid MAX_PAGE_SIZE definition
#endif

/*
 * If MIN_PAGE_SIZE and MAX_PAGE_SIZE are not equal, then we must use
 * non-constant PAGE_SIZE, et al for LKMs.
 */
#if (MIN_PAGE_SIZE != MAX_PAGE_SIZE)
#define	__uvmexp_pagesize
#if defined(_LKM) || defined(_MODULE)
#undef PAGE_SIZE
#undef PAGE_MASK
#undef PAGE_SHIFT
#endif
#endif

/*
 * Now provide PAGE_SIZE, PAGE_MASK, and PAGE_SHIFT if we do not
 * have ones that are compile-time constants.
 */
#if !defined(PAGE_SIZE)
extern const int *const uvmexp_pagesize;
extern const int *const uvmexp_pagemask;
extern const int *const uvmexp_pageshift;
#define	PAGE_SIZE	(*uvmexp_pagesize)	/* size of page */
#define	PAGE_MASK	(*uvmexp_pagemask)	/* size of page - 1 */
#define	PAGE_SHIFT	(*uvmexp_pageshift)	/* bits to shift for pages */
#endif /* PAGE_SIZE */

#endif /* _KERNEL */

/*
 * CTL_VM identifiers
 */
#define	VM_METER	1		/* struct vmmeter */
#define	VM_LOADAVG	2		/* struct loadavg */
#define	VM_UVMEXP	3		/* struct uvmexp */
#define	VM_NKMEMPAGES	4		/* kmem_map pages */
#define	VM_UVMEXP2	5		/* struct uvmexp_sysctl */
#define	VM_ANONMIN	6
#define	VM_EXECMIN	7
#define	VM_FILEMIN	8
#define	VM_MAXSLP	9
#define	VM_USPACE	10
#define	VM_ANONMAX	11
#define	VM_EXECMAX	12
#define	VM_FILEMAX	13

#define	VM_MAXID	14		/* number of valid vm ids */

#define	CTL_VM_NAMES { \
	{ 0, 0 }, \
	{ "vmmeter", CTLTYPE_STRUCT }, \
	{ "loadavg", CTLTYPE_STRUCT }, \
	{ "uvmexp", CTLTYPE_STRUCT }, \
	{ "nkmempages", CTLTYPE_INT }, \
	{ "uvmexp2", CTLTYPE_STRUCT }, \
	{ "anonmin", CTLTYPE_INT }, \
	{ "execmin", CTLTYPE_INT }, \
	{ "filemin", CTLTYPE_INT }, \
	{ "maxslp", CTLTYPE_INT }, \
	{ "uspace", CTLTYPE_INT }, \
	{ "anonmax", CTLTYPE_INT }, \
	{ "execmax", CTLTYPE_INT }, \
	{ "filemax", CTLTYPE_INT }, \
}

#ifndef ASSEMBLER
/*
 *	Convert addresses to pages and vice versa.
 *	No rounding is used.
 */
#ifdef _KERNEL
#define	atop(x)		(((paddr_t)(x)) >> PAGE_SHIFT)
#define	ptoa(x)		(((paddr_t)(x)) << PAGE_SHIFT)

/*
 * Round off or truncate to the nearest page.  These will work
 * for either addresses or counts (i.e., 1 byte rounds to 1 page).
 */
#define	round_page(x)	(((x) + PAGE_MASK) & ~PAGE_MASK)
#define	trunc_page(x)	((x) & ~PAGE_MASK)

/*
 * Set up the default mapping address (VM_DEFAULT_ADDRESS) according to:
 *
 * USE_TOPDOWN_VM:	a kernel option to enable on a per-kernel basis
 *			which only be used on ports that define...
 * __HAVE_TOPDOWN_VM:	a per-port option to offer the topdown option
 *
 * __USE_TOPDOWN_VM:	a per-port option to unconditionally use it
 *
 * if __USE_TOPDOWN_VM is defined, the port can specify a default vm
 * address, or we will use the topdown default from below.  If it is
 * NOT defined, then the port can offer topdown as an option, but it
 * MUST define the VM_DEFAULT_ADDRESS macro itself.
 */
#if defined(USE_TOPDOWN_VM) || defined(__USE_TOPDOWN_VM)
# if !defined(__HAVE_TOPDOWN_VM) && !defined(__USE_TOPDOWN_VM)
#  error "Top down memory allocation not enabled for this system"
# else /* !__HAVE_TOPDOWN_VM && !__USE_TOPDOWN_VM */
#  define __USING_TOPDOWN_VM
#  if !defined(VM_DEFAULT_ADDRESS)
#   if !defined(__USE_TOPDOWN_VM)
#    error "Top down memory allocation not configured for this system"
#   else /* !__USE_TOPDOWN_VM */
#    define VM_DEFAULT_ADDRESS(da, sz) \
	trunc_page(VM_MAXUSER_ADDRESS - MAXSSIZ - (sz))
#   endif /* !__USE_TOPDOWN_VM */
#  endif /* !VM_DEFAULT_ADDRESS */
# endif /* !__HAVE_TOPDOWN_VM && !__USE_TOPDOWN_VM */
#endif /* USE_TOPDOWN_VM || __USE_TOPDOWN_VM */

#if !defined(__USING_TOPDOWN_VM)
# if defined(VM_DEFAULT_ADDRESS)
#  error "Default vm address should not be defined here"
# else /* VM_DEFAULT_ADDRESS */
#  define VM_DEFAULT_ADDRESS(da, sz) round_page((vaddr_t)(da) + (vsize_t)maxdmap)
# endif /* VM_DEFAULT_ADDRESS */
#endif /* !__USING_TOPDOWN_VM */

extern int		ubc_nwins;	/* number of UBC mapping windows */
extern int		ubc_winshift;	/* shift for a UBC mapping window */
extern u_int		uvm_emap_size;	/* size of emap */

#else
/* out-of-kernel versions of round_page and trunc_page */
#if !defined(__minix)
#define	round_page(x) \
	((((vaddr_t)(x) + (vm_page_size - 1)) / vm_page_size) * \
	    vm_page_size)
#define	trunc_page(x) \
	((((vaddr_t)(x)) / vm_page_size) * vm_page_size)
#else
/* LSC: Minix always uses the same definition of those. */
#define	round_page(x)	(((x) + PAGE_MASK) & ~PAGE_MASK)
#define	trunc_page(x)	((x) & ~PAGE_MASK)
#endif /* !defined(__minix) */

#endif /* _KERNEL */

/*
 * typedefs, necessary for standard UVM headers.
 */

typedef unsigned int uvm_flag_t;

typedef int vm_inherit_t;	/* XXX: inheritance codes */
typedef off_t voff_t;		/* XXX: offset within a uvm_object */
typedef voff_t pgoff_t;		/* XXX: number of pages within a uvm object */

#endif /* ASSEMBLER */
#endif /* _VM_PARAM_ */
