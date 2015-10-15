/*	$NetBSD: __clone.S,v 1.1 2014/09/03 19:34:26 matt Exp $	*/

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
__RCSID("$NetBSD: __clone.S,v 1.1 2014/09/03 19:34:26 matt Exp $")
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
	l.sfeqi	r3,0
	l.bf	2f
	l.sfeqi	r4,0
	l.bf	2f

	l.or	r7,r3,r0	/* Save fn in r7. */
	l.or	r3,r5,r0
	_DOSYSCALL(__clone)	/* (flags, stack) */
	l.bf	_C_LABEL(__cerror)	/* error... */
	l.nop

	l.sfeqi	r11,0
	l.bf	1f		/* We're the parent, just return. */
	l.nop
	l.jr	lr
	l.nop

1:	l.or	r3,r6,r0	/* arg */
	l.jalr	r7		/* Call the clone's entry point. */
	l.nop

#ifdef __PIC__
	// We don't need to save r9 or r16 since we aren't ever coming back
	PIC_GOTSETUP(r16)
#endif
	l.jal	plt(_C_LABEL(_exit))
	l.nop

2:
	l.addi	r11,r0,EINVAL
	l.j	_C_LABEL(__cerror)
	l.nop
END(__clone)
