/*	$NetBSD: SYS.h,v 1.9 2014/03/06 19:02:58 skrll Exp $	*/

/*	$OpenBSD: SYS.h,v 1.9 2001/09/20 20:52:09 millert Exp $	*/

/*
 * Copyright (c) 1998-2002 Michael Shalayeff
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF MIND
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/syscall.h>
#include <machine/asm.h>
#include <machine/vmparam.h>
#undef _LOCORE
#define _LOCORE
#include <machine/frame.h>

#define	SYSENTRY(x)	LEAF_ENTRY(x)
#define	SYSEXIT(x)	EXIT(x)

#define	SYSCALL(x)				!\
	stw	%rp, HPPA_FRAME_ERP(%sr0,%sp)	!\
	ldil	L%SYSCALLGATE, %r1		!\
	ble	4(%sr2, %r1)			!\
	ldi	__CONCAT(SYS_,x), %t1		!\
	.import __cerror, code			!\
	comb,<>	%r0, %t1, __cerror		!\
	ldw	HPPA_FRAME_ERP(%sr0,%sp), %rp

#define	PSEUDO(x,y)				!\
SYSENTRY(x)					!\
	SYSCALL(y)				!\
	bv	%r0(%rp)			!\
	nop					!\
SYSEXIT(x)

#define	PSEUDO_NOERROR(x,y)			!\
SYSENTRY(x)					!\
	stw	%rp, HPPA_FRAME_ERP(%sr0,%sp)	!\
	ldil	L%SYSCALLGATE, %r1		!\
	ble	4(%sr2, %r1)			!\
	ldi	__CONCAT(SYS_,y), %t1		!\
	ldw	HPPA_FRAME_ERP(%sr0,%sp), %rp	!\
	bv	%r0(%rp)			!\
	nop					!\
SYSEXIT(x)

#define RSYSCALL(x)		PSEUDO(x,x)
#define	RSYSCALL_NOERROR(x)	PSEUDO_NOERROR(x,x)

#ifdef WEAK_ALIAS
#define WSYSCALL(weak,strong)		!\
	WEAK_ALIAS(weak,strong)		!\
	PSEUDO(strong,weak)
#else
#define WSYSCALL(weak,strong)		!\
	PSEUDO(weak,weak)
#endif
