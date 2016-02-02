/* $NetBSD: memcmp.S,v 1.1 2014/09/03 19:34:25 matt Exp $ */

/*-
 * Copyright (c) 2014 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas of 3am Software Foundry.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *
 *
 * int memcmp(const char *s1, const char *s2, size_t n);
 *
 *	for (; n-- != 0; s1++, s2++) {
 *		if (*s1 < *s2)
 *			return -1;
 *		if (*s1 > *s2)
 *			return 1;
 *	}
 *	return 0;
 *
 * Return:	((s1 > s2) ? 1 : (s1 < s2) ? -1 : 0)
 *
 * ==========================================================================
 */

#include <machine/asm.h>

        .text
        .align 4
/* LINTSTUB: Func: void *memcmp(const void *, const void *, size_t) */
ENTRY(memcmp)

	/*
	 * Check count passed in R5. If zero, return 0; otherwise continue.
	 */
	l.sfeqi	r5, 0			/* nothing to compare?		*/
	l.bf	.Lret_0			/*   yes, return equality	*/
	l.nop

#ifdef _KERNEL
	l.sfeqi	r5, 6			/* less than two words?		*/
	l.bnf	.Lsixbyte_compare	/*   yes, just compare by bytes */
	l.nop
#endif

	l.sfgesi r5, 7			/* less than two words?		*/
	l.bnf	.Lbyte_compare		/*   yes, just compare by bytes */
	l.nop

	l.xor	r6, r3, r4		/* check alignment compatibility */
	l.andi	r6, r6, 3		/* only care about the two bits */
	l.sfeqi	r6, 0			/* same alignment? */
	l.bnf	.Lmisaligned		/*   no, avoid alignment errors */
	l.nop

	/*
	 * At this point, we know we read the data via word accesses.
	 */

	l.andi	r7, r3, 3		/* check alignment		*/
	l.sfeqi	r7, 0			/* word aligned?		*/
	l.bf	.Lword_compare		/*   yes, it is.		*/

	l.sub	r3, r3, r7		/* align string 1		*/
	l.sub	r4, r4, r7		/* align string 2		*/
	l.add	r5, r5, r7		/* pad length			*/

	l.lwz	r15, 0(r3)		/* load word from s1		*/
	l.lwz	r17, 0(r4)		/* load word from s2		*/

	l.slli	r7, r7, 3		/* bytes to bits		*/
	l.sll	r15, r15, r7		/* shift away leading bytes	*/
	l.sll	r17, r17, r7		/* shift away leading bytes	*/
	l.j	.Lword_compare		/* now we can compare them	*/
	l.nop

.Lword_loop:
	l.lwz	r15, 0(r3)		/* load s1 word			*/
	l.lwz	r17, 0(r4)		/* load s2 word			*/
.Lword_compare:
	l.sfeq	r15, r17		/* compare s1 and s2 words	*/
	l.bnf	.Lall_done		/*   different? we're done	*/

	l.addi	r3, r3, 4		/* advance s1 one word		*/
	l.addi	r4, r4, 4		/* advance s2 one word		*/
	l.addi	r5, r5, -4		/* decrement one word		*/
	l.sfgtsi r5, 4			/* at least more than a word?	*/
	l.bf	.Lword_loop		/*    yes, loop around		*/
	l.nop
	l.sfeqi	r5, 0			/* nothing left?		*/
	l.bf	.Lret_0			/*   yes, return equality	*/
	l.nop

	/*
	 * Fall through to handle the last word
	 */

	l.sub	r3, r0, r5		/* If count <= 4, handle 	*/
	l.andi	r3, r3, 3		/* mask off low 2 bits		*/
	l.slli	r3, r3, 3		/* count *= 8			*/
	l.srl	r15, r15, r3		/* discard extra s1 bytes	*/
	l.srl	r17, r17, r3		/* discard extra s2 bytes	*/

	l.sfeq	r17, r15		/* compare result		*/
	l.bnf	.Lall_done
.Lret_0:
	l.addi	r11, r0, 0
	l.jr	lr
	l.nop

/*
 * The two string don't have the same word alignment.
 */
.Lmisaligned:
	l.sfeqi	r6, 2			/* check for halfword alignment */
	l.bnf	.Lbyte_compare
	l.nop
	l.andi	r7, r3, 1
	l.sfeqi	r7, 0
	l.bf	.Lhalfword_loop
	l.nop
	l.addi	r5, r5, 1
	l.addi	r3, r3, -1
	l.addi	r4, r4, -1
	l.lbz	r15, 1(r3)
	l.lbz	r17, 1(r4)
	l.j	.Lhalfword_compare
	l.nop
.Lhalfword_loop:
	l.lhz	r15, 0(r3)
	l.lhz	r17, 0(r4)
.Lhalfword_compare:
	l.sfeq	r15, r17
	l.bnf	.Lall_done
	l.nop
	l.addi	r3, r3, 2
	l.addi	r4, r4, 2
	l.addi	r5, r5, -2
	l.sfgesi r5, 2
	l.bf	.Lhalfword_loop
	l.nop

.Lbyte_compare:
	l.addi	r5, r5, -1
	l.sfgesi r5, 0
	l.bnf	.Lret_0
	l.nop
	l.lbz	r15, 0(r3)
	l.lbz	r17, 0(r4)
	l.addi	r3, r3, 1
	l.addi	r4, r4, 1
	l.sfeq	r15, r17
	l.bf	.Lbyte_compare
	l.nop

.Lall_done:
	l.sub	r11, r15, r17		/* subtract s2 from s1		*/
	l.srai	r11, r11, 30		/* replicate sign bit thru bit 1 */
	l.ori	r11, r11, 1		/* make sure bit 0 is set	*/
	l.jr	lr
	l.nop

#ifdef _KERNEL
.Lsixbyte_compare:
	l.or	r7, r3, r4
	l.andi	r7, r7, 1
	l.sfeqi	r7, 0
	l.bnf	.Lbyte_compare
	l.nop
	l.lhz	r15, 0(r3)
	l.lhz	r17, 0(r4)
	l.sfeq	r15, r17
	l.bnf	.Lall_done
	l.nop
	l.lhz	r15, 2(r3)
	l.lhz	r17, 2(r4)
	l.sfeq	r15, r17
	l.bnf	.Lall_done
	l.nop
	l.lhz	r15, 4(r3)
	l.lhz	r17, 4(r4)
	l.sfeq	r15, r17
	l.bnf	.Lall_done
	l.nop
	l.addi	r11, r0, 0
	l.jr	lr
	l.nop
#endif
END(memcmp)
