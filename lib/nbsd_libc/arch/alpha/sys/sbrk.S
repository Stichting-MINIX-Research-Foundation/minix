/* $NetBSD: sbrk.S,v 1.9 2001/05/07 17:19:18 kleink Exp $ */

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
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

#include "SYS.h"

	.globl	_end

#ifdef WEAK_ALIAS
WEAK_ALIAS(sbrk, _sbrk)
#endif

	.data
EXPORT(__curbrk)
	.quad	_end

	.text
LEAF(_sbrk, 1)
	br	pv, L1				/* XXX profiling */
L1:	LDGP(pv)
	ldq	a1, __curbrk
	addq	a0, a1, a0
	CALLSYS_ERROR(break)
	stq	a0, __curbrk
	mov	a1, v0
	RET
END(_sbrk)
