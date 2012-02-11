/*	$NetBSD: __clone.S,v 1.4 2011/01/16 02:43:10 matt Exp $	*/

/*-
 * Copyright (c) 2001 Tsubai Masanari.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/errno.h>
#include "SYS.h"

#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: __clone.S,v 1.4 2011/01/16 02:43:10 matt Exp $")
#endif /* LIBC_SCCS && !lint */

#ifdef WEAK_ALIAS
WEAK_ALIAS(clone, __clone)
#endif

/*
 * int __clone(int (*fn)(void *), void *stack, int flags, void *arg);
 */
ENTRY(__clone)
	/*
	 * Sanity checks: func and stack may not be NULL.
	 */
	cmpwi	%r3,0
	beq	1f
	cmpwi	%r4,0
	beq	1f

	mr	%r7,%r3		/* Save fn in r7. */
	mr	%r3,%r5
	_DOSYSCALL(__clone)	/* (flags, stack) */
	bso	2f		/* error... */

	cmpwi	%r3,0
	bnelr			/* We're the parent, just return. */

	mtlr	%r7		/* fn */
	mr	%r3,%r6		/* arg */
	blrl			/* Call the clone's entry point. */

#ifdef PIC
	PIC_TOCSETUP(__clone, %r30)	/* exit won't return so blow away r30 */
#endif
	bl	PIC_PLT(_C_LABEL(_exit))

1:
	li	%r3,EINVAL
2:
	b	_C_LABEL(__cerror)
END(__clone)
