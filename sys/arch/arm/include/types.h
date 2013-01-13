/*	$NetBSD: types.h,v 1.23 2012/08/12 05:05:47 matt Exp $	*/

/*
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
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
 *	from: @(#)types.h	7.5 (Berkeley) 3/9/91
 */

#ifndef	_ARM_TYPES_H_
#define	_ARM_TYPES_H_

#include <sys/cdefs.h>
#include <sys/featuretest.h>
#include <arm/int_types.h>

#if defined(_KERNEL)
typedef struct label_t {	/* Used by setjmp & longjmp */
        int val[11];
} label_t;
#endif
         
/* NB: This should probably be if defined(_KERNEL) */
#if defined(_NETBSD_SOURCE)
typedef	unsigned long	vm_offset_t;	/* depreciated */
typedef	unsigned long	vm_size_t;	/* depreciated */

typedef unsigned long	paddr_t;
typedef unsigned long	psize_t;
typedef unsigned long	vaddr_t;
typedef unsigned long	vsize_t;
#define	PRIxPADDR	"lx"
#define	PRIxPSIZE	"lx"
#define	PRIuPSIZE	"lu"
#define	PRIxVADDR	"lx"
#define	PRIxVSIZE	"lx"
#define	PRIuVSIZE	"lu"
#endif

typedef int		register_t, register32_t;
#define	PRIxREGISTER	"x"

typedef unsigned long	pmc_evid_t;
#define PMC_INVALID_EVID	(-1)
typedef unsigned long	pmc_ctr_t;

/*
 * This should have always been an 8-bit type, but since it's been exposed
 * to user-space, we don't want ABI breakage there.
 */
#if defined(_KERNEL)
typedef volatile unsigned char	__cpu_simple_lock_t;
#else
typedef	volatile int		__cpu_simple_lock_t;
#endif /* _KERNEL */

#define	__SIMPLELOCK_LOCKED	1
#define	__SIMPLELOCK_UNLOCKED	0

#define	__HAVE_SYSCALL_INTERN
#define	__HAVE_NEW_STYLE_BUS_H
#define	__HAVE_MINIMAL_EMUL
#define __HAVE_CPU_DATA_FIRST
#define	__HAVE___LWP_GETPRIVATE_FAST
#define	__HAVE_COMMON___TLS_GET_ADDR
#if !defined(__minix)
#define	__HAVE_TLS_VARIANT_I
#endif /* !defined(__minix) */

#if defined(_KERNEL) || defined(_KMEMUSER)
#define	PCU_FPU			0
#define	PCU_UNIT_COUNT		1
#endif

#if defined(_KERNEL)
#define	__HAVE_RAS
#endif

#endif	/* _ARM_TYPES_H_ */
