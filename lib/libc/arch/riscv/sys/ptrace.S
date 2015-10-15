/*	$NetBSD: ptrace.S,v 1.2 2015/03/27 06:44:28 matt Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
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
 */

#include "SYS.h"

#if defined(LIBC_SCCS) && !defined(lint)
	RCSID("$NetBSD: ptrace.S,v 1.2 2015/03/27 06:44:28 matt Exp $")
#endif /* LIBC_SCCS and not lint */

#define	XCALLFRAME_SIZ	(8*SZREG)
#define	XCALLFRAME_RA	(0*SZREG)
#define	XCALLFRAME_A0	(1*SZREG)
#define	XCALLFRAME_A1	(2*SZREG)
#define	XCALLFRAME_A2	(3*SZREG)
#define	XCALLFRAME_A3	(4*SZREG)

ENTRY(ptrace)
#ifdef _REENTRANT
	addi	sp, sp, -XCALLFRAME_SIZ
	REG_S	ra, XCALLFRAME_RA(sp)
	REG_S	a0, XCALLFRAME_A0(sp)
	REG_S	a1, XCALLFRAME_A1(sp)
	REG_S	a2, XCALLFRAME_A2(sp)
	REG_S	a3, XCALLFRAME_A3(sp)

	call	PLT(_C_LABEL(__errno))

	INT_S	zero, 0(a0)

	REG_L	a3, XCALLFRAME_A3(sp)
	REG_L	a2, XCALLFRAME_A2(sp)
	REG_L	a1, XCALLFRAME_A1(sp)
	REG_L	a0, XCALLFRAME_A0(sp)
	REG_L	ra, XCALLFRAME_RA(sp)
	addi	sp, sp, XCALLFRAME_SIZ

#else
	auipc	t0, %hi(_C_LABEL(errno))
	INT_S	zero, %lo(_C_LABEL(errno))(t0)
#endif

	SYSTRAP(ptrace)
	JUMP_TO_CERROR()	/* error */
	ret			/* success */
END(ptrace)
