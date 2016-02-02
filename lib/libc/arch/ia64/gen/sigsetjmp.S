/*	$NetBSD: sigsetjmp.S,v 1.1 2015/04/17 12:51:05 martin Exp $	*/

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

#include <machine/asm.h>
RCSID("$NetBSD: sigsetjmp.S,v 1.1 2015/04/17 12:51:05 martin Exp $");

#include <machine/setjmp.h>

/*
 * C library -- sigsetjmp, siglongjmp
 *
 *	siglongjmp(a,v)
 * will generate a "return(v)" from
 * the last call to
 *	sigsetjmp(a, mask)
 * by restoring registers from the stack.
 * If `mask' is non-zero, the previous signal
 * state will be restored.
 */

ENTRY(__sigsetjmp14, 2)
	add	r14=J_SIGMASK,in0		// place to save mask
	cmp.ne	p6,p7=0,in1			// save signal state?
	;;
	st8	[r14]=in1			// save mask value
(p6)	br.cond.dptk.many __setjmp14
(p7)	br.cond.dpnt.many _setjmp		
END(__sigsetjmp14)

ENTRY(__siglongjmp14, 2)
	add	r14=J_SIGMASK,in0		// address of mask value
	;;
	ld8	r14=[r14]
	;;
	cmp.ne	p6,p7=0,r14			// did we save signals?
(p6)	br.cond.dptk.many __longjmp14
(p7)	br.cond.dpnt.many _longjmp		
END(__siglongjmp14)
