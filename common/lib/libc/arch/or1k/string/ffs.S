/*	$NetBSD: ffs.S,v 1.1 2014/09/03 19:34:25 matt Exp $ */

/*-
 * Copyright (C) 2001	Martin J. Laubach <mjl@NetBSD.org>
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
/*----------------------------------------------------------------------*/

#include <machine/asm.h>

__RCSID("$NetBSD: ffs.S,v 1.1 2014/09/03 19:34:25 matt Exp $")

ENTRY_NP(__ffssi2)
#ifdef _LP64
	l.extwz	r3, r3
#endif
	l.ff1	r11, r3
	l.jr	lr
	l.nop
END(__ffssi2)
STRONG_ALIAS(ffs, __ffssi2)

ENTRY_NP(__ffsdi2)
#ifdef _LP64
	l.ff1	r11, r3
	l.jr	lr
	l.nop
#else
	l.sfeqi	r4, 0
	l.bf	1f
	l.nop

	l.ff1	r11, r4
	l.jr	lr
	l.nop

1:	l.ff1	r11, r3
	l.addi	r11, r11, 32
	l.jr	lr
	l.nop
#endif
END(__ffsdi2)
