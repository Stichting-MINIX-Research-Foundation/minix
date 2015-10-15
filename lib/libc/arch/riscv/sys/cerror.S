/*	$NetBSD: cerror.S,v 1.3 2015/03/27 23:23:14 matt Exp $	*/

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
	RCSID("$NetBSD: cerror.S,v 1.3 2015/03/27 23:23:14 matt Exp $")
#endif /* LIBC_SCCS and not lint */

	.protected	_C_LABEL(__cerror)

	.globl	_C_LABEL(__errno)
ENTRY_NP(__cerror)
#ifdef _REENTRANT
	addi		sp, sp, -CALLFRAME_SIZ
	REG_S		ra, CALLFRAME_RA(sp)	# save return address
	INT_S		a0, CALLFRAME_S0(sp)	# save errno value

	call		PLT(_C_LABEL(__errno))	# locate address of errno

	INT_L		t0, CALLFRAME_S0(sp)	# retrieve errno value
	REG_L		ra, CALLFRAME_RA(sp)	# retrieve return address
	INT_S		t0, 0(a0)		# update errno value
	addi		sp, sp, CALLFRAME_SIZ
#else
	aiupc		a1, %hi(_C_LABEL(errno))
	INT_S		a0, %lo(_C_LABEL(errno))(a1)
#endif /* _REENTRANT */
	li		a0, -1
	li		a1, -1
	ret
END(__cerror)
