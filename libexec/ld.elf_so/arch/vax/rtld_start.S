/*	$NetBSD: rtld_start.S,v 1.24 2014/08/17 16:57:37 matt Exp $	*/

/*
 * Copyright 1996 Matt Thomas <matt@3am-software.com>
 * Portions copyright 2002, 2003 Charles M. Hannum <root@ihack.net>
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <machine/asm.h>

/* R9 contains the address of PS_STRINGS and since its caller saved,
 * we can just use it.  R6 has a backup copy of the stack pointer which
 * we can use as well.
 */
ENTRY(_rtld_start, 0)
	/* Allocate space on the stack for the cleanup and obj_main
	 * entries that _rtld() will provide for us.
	 */
	clrl	%fp
	subl2	$8,%sp

	movab	_DYNAMIC,%r0
	subl3	_GLOBAL_OFFSET_TABLE_,%r0,%r10
	pushl	%r10		/* relocbase */
	pushl	%r0		/* &_DYNAMIC */
	calls	$2,_rtld_relocate_nonplt_self

	pushl	%r10		/* relocbase */
	pushal	4(%sp)		/* sp */
	calls	$2,_rtld	/* entry = _rtld(sp, relocbase) */

	movq	(%sp)+,%r7	/* grab cleanup and obj_main into %r7/%r8 */
	jmp	2(%r0)		/* jump to entry point + 2 */
END(_rtld_start)

/*
 * Lazy binding entry point, called via PLT via JMP into pltgot[1].
 * SP+4: address to relocation offset
 * SP+0: obj entry points
 */
ALTENTRY(_rtld_bind_start)
	pushl	%r1		/* need to preserve r1 */
	movq	-8(%fp),%r0	/* get addresses of plt.got & reloc index */
	pushl	(%r1)		/* push relocation offset */
	pushl	%r0		/* push address of obj entry */
	calls	$2,_rtld_bind

	/*
	 * This code checks to see if we got called via a call{s,g} $n,*pcrel32
	 * This is by far the most common case (a call indirectly via the PLT).
	 */
	subl3	$7,16(%fp),%r1	/* return address */
	bicb3	$1,(%r1),%r2	/* fetch opcode of instruction */
	cmpb	$0xfa,%r2	/* is it calls/callg */
	jneq	20f		/*   no it isn't */
	cmpb	$0xff,2(%r1)	/* and deferred 32-bit PC displacement? */
	jneq	20f		/*   no it isn't */

	/*
	 * This makes sure the longword with the PLT's address has been updated
	 * to point to the routine's address.  If it hasn't, then returning
	 * would put us in an infinite loop.  Instead we punt and fake up a
	 * callframe.
	 */
	movl	3(%r1),%r3	/* get displacement */
	addl2	16(%fp),%r3	/* add ending location */
	cmpl	(%r3),%r0	/* does it contain the routine address? */
#ifdef DEBUG
	jneq	30f		/*   no it doesn't, die */
#else
	jneq	20f		/*   no it doesn't, go fake a new callframe */
#endif

11:	movl	%r1,16(%fp)	/* backup to the calls/callg */
	jbc	$29,4(%fp),12f	/* skip if this was a callg */
	clrl	(%ap)		/* clear argument count */
12:	movl	(%sp)+,%r1	/* restore r1 */
	ret			/* return and redo the call */

#if 1
20:
	/*
	 * Since the calling standard says only r6-r11 should be saved,
	 * that simplies things for us.  That means we can use r0-r5 as
	 * temporaries without worrying about preserving them.  This means
	 * can hold the current fixed callframe in r2-r5 as we build the
	 * callframe without having to worry about overwriting the existing
	 * callframe.
	 */
	extzv	$0,$12,(%r0),%r1/* get routine's save mask */
	bitw	$0x3f,%r1	/* does the routine use r0-r5? */
	jneq	30f		/*   yes, that sucks */
	jbc	$29,4(%fp),27f	/* handle callg */
	movq	4(%fp),%r2	/* fetch callframe status & saved AP */
	movq	12(%fp),%r4	/* fetch callframe saved FP & PC */
	insv	%r1,$16,$12,%r2	/* update save mask */
	movl	(%sp)+,%fp	/* use fp to keep saved r1 */
	movl	%ap,%sp		/* reset stack to top of callframe */
22:	pushr	%r1		/* push registers */
	movq	%r4,-(%sp)	/* push callframe saved FP & PC */
	movq	%r2,-(%sp)	/* push callframe status & saved AP */
	pushl	$0		/* push condition handler */
	movl	%fp,%r1		/* restore r1 */
	movl	%sp,%fp		/* sp == fp now */
#if 1
	jmp	2(%r0)		/* jump past entry mask */
#else
	/*
	 * More correct but IV/DV are never set so ignore doing this for now.
	 */
	movpsl	-(%sp)		/* push PSL */
	clrb	(%sp)		/* clear user flags */
	jbc	$14,(%r0),24f	/* IV need to be set? */
	bisb2	$0x20,(%sp)	/*   yes, set it. */
24:	jbc	$15,(%r0),25f	/* DV need to be set? */
	bisb2	$0x80,(%sp)	/*   yes, set it. */
25:	pushab	2(%r0)		/* push address of first instruction */
	rei			/* and go to it (updating PSW) */
#endif

	/*
	 * Count how many registers are being used for callg.
	 */
27:	movl	$0x32212110,%r3	/* bit counts */
	extzv	$6,$3,%r1,%r2	/* extract bits 6-8 */
	ashl	$2,%r2,%r2	/* shift by 2 */
	extzv	%r2,$4,%r3,%r4	/* extract count */
	extzv	$9,$3,%r1,%r2	/* extract bits 9-11 */
	ashl	$2,%r2,%r2	/* shift by 2 */
	extzv	%r2,$4,%r3,%r5	/* extract count */
	movq	4(%fp),%r2	/* fetch callframe status & saved AP */
	insv	%r1,$16,$12,%r2	/* update save mask */
	addl3	%r4,%r5,%r1	/* add counts and discard them */
	movq	12(%fp),%r4	/* fetch callframe saved FP & PC */
	moval	20(%fp)[%r1],%sp/* pop callframe */
	extzv	$16,$12,%r2,%r1	/* get save mask back */
	jbr	22b		/* now build the new callframe */

30:	
	calls	$0,_C_LABEL(_rtld_die)
#else
	/*
	 * Check to see if called via call? $n,w^off(reg)
	 */
20:	addl2	$2,%r1		/* 16-bit displacement */
	bicb3	$1,(%r1),%r2	/* fetch opcode of instruction */
	cmpb	$0xfa,%r2	/* is it calls/callg */
	jneq	30f		/*   no it isn't */
	bicb3	$0x1f,2(%r1),%r3/* extract addressing mode */
	cmpb	$0xc0,%r3	/* 16-bit displacement? */
	jeql	11b		/*   yes, redo the call */
	halt

	/*
	 * Check to see if called via call? $n,b^off(reg)
	 */
30:	incl	%r1		/* 8-bit displacement */
	bicb3	$1,(%r1),%r2	/* fetch opcode of instruction */
	cmpb	$0xfa,%r2	/* is it calls/callg */
	jneq	40f		/*   no it isn't */
	bicb3	$0x1f,2(%r1),%r3/* extract addressing mode */
	cmpb	$0xa0,%r3	/* 8-bit displacement? */
	jeql	11b		/*   yes, redo the call */
	halt

	/*
	 * Check to see if called via call? $n,(reg)
	 */
40:	incl	%r1		/* no displacement */
	bicb3	$1,(%r1),%r2	/* fetch opcode of instruction */
	cmpb	$0xfa,%r2	/* is it calls/callg */
	jeql	41f		/*   yes it is */
	halt			/*   no, die die die */
41:	bicb3	$0x0f,2(%r1),%r2/* extract addressing mode */
	bicb3	$0xf0,2(%r1),%r3/* extract register */
	extzv	$0,$12,6(%fp),%r4/* extract saved mask */
	cmpb	$0x60,%r2	/* register deferred? */
	jeql	42f		/*   yes, deal with it */
	cmpb	$0x90,%r2	/* autoincrement deferred? */
	jeql	70f		/*   yes, deal with it */
	halt			/*   no, die die die */

42:	cmpw	%r4,$0xffc	/* did we save r2-r11? */
	jneq	50f		/*   no, deal with it */
	jbc	%r3,%r4,43f	/* is the register in the saved mask? */

	/*
	 * We saved r2-r11, so it's easy to replace the saved register with
	 * the right value by indexing into saved register (offset by 8).
	 */
	movl	%r0,(20-8)(%fp)[%r3] /* replace address in saved registers */
	jbr	11b		/* go back and redo call */
	/*
	 * Must have been called via r0 or r1 which are saved locally.
	 * So move the routine address in the appropriate slot on the stack.
	 */
43:	movl	%r0,(%sp)[%r3]
	jbr	11b		/* go back and redo call */

50:	jbs	%r3,%r4,60f	/* is the register in the saved mask? */
	jbs	%r3,$0x3f,43b	/* is it r0-r5? */
	/*
	 * The register used for the call was not saved so we need to move
	 * the new function address into it so the re-call will use the new
	 * address.
	 */
	pushl	%r0		/* save function address on the stack */
	ashl	%r5,$1,%r0	/* create a bitmask for the register */
	popr	%r0		/* pop it off the stack. */
	jbr	11b		/* and redo the call */

60:	clrl	%r2		/* starting offset into saved registers */
	clrl	%r5		/* start with register 0 */

61:	cmpl	%r2,%r3		/* is the register to save? */
	jneq	62f		/*   no, advance to next */
	movl	%r0,20(%fp)[%r5]/*   yes, save return address in saved reg */
	jbr	11b		/*        and return the call */
62:	jbc	%r5,%r4,63f	/* is this register saved? */
	incl	%r5		/*   yes, account for it */
63:	incl	%r2		/* increment register number */
	jbr	61b		/* and loop */

70:	cmpb	%r3,$12
	blss	71f
	halt

71:	cmpw	%r4,$0xffc	/* did we save r2-r11? */
	jneq	72f		/*   no, deal with it */
	subl2	$4,(20-8)(%fp)[%r3] /* backup incremented register */
	jbr	11b		/* and redo the call.

72:	jbs	%r3,%r4,80f
	jbs	%r3,%3f,74f
	ashl	%r5,$1,%r0	/* create a bitmask for the register */
	pushr	%r0		/* pop it off the stack. */
	subl2	$4,(%sp)	/* backup incremented register */
	popr	%r0		/* pop it off the stack. */
	jbr	11b		/* and redo the call.

73:	subl2	%4,(%sp)[%r3]	/* backup incremented register */
	jbr	11b		/* and redo the call.

80:	clrl	%r2		/* starting offset into saved registers */
	clrl	%r5		/* start with register 0 */

81:	cmpl	%r2,%r3		/* is the register to save? */
	jneq	82f		/*   no, advance to next */
	subl	$4,20(%fp)[%r5]	/*   yes, backup incremented register */
	jbr	11b		/*        and return the call */
82:	jbc	%r5,%r4,83f	/* is this register saved? */
	incl	%r5		/*   yes, account for it */
83:	incl	%r2		/* increment register number */
	jbr	81b		/* and loop */
#endif
END(_rtld_bind_start)
