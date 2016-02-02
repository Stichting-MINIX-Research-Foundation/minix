/* $NetBSD: memmove.S,v 1.1 2014/09/03 19:34:25 matt Exp $ */

/* stropt/memmove.S, pl_string_common, pl_linux 10/11/04 11:45:37
 * ==========================================================================
 * Optimized memmove implementation for IBM PowerPC 405/440.
 *
 *	Copyright (c) 2003, IBM Corporation
 *	All rights reserved.		
 *					
 *	Redistribution and use in source and binary forms, with or	
 *	without modification, are permitted provided that the following 
 *	conditions are met:						
 *									
 *	* Redistributions of source code must retain the above	
 *	copyright notice, this list of conditions and the following 
 *	disclaimer.						 
 *	* Redistributions in binary form must reproduce the above	
 *	copyright notice, this list of conditions and the following 
 *	disclaimer in the documentation and/or other materials	
 *	provided with the distribution.				
 *	* Neither the name of IBM nor the names of its contributors	
 *	may be used to endorse or promote products derived from this
 *	software without specific prior written permission.	 
 *									
 *	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND		
 *	CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,	
 *	INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF	
 *	MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE	
 *	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS	
 *	BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, 
 *	OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,		
 *	PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR	
 *	PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
 *	OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT	
 *	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE	
 *	USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *										
 * ==========================================================================
 *
 * Function: Move memory area (handles overlapping regions)
 *
 *		void *memmove(void * dest, const void * src, int n) 
 *	
 * Input:	r3 - destination address 
 *		r4 - source address 
 *		r5 - byte count
 * Output:	r11 - destination address
 *
 * ==========================================================================
 */

#include <machine/asm.h>

#ifdef _BCOPY
/* bcopy = memcpy/memmove with arguments reversed. */
/* LINTSTUB: Func: void bcopy(void *, void *, size_t) */
ENTRY(bcopy)
	l.or	r6, r3, r0		/* swap src/dst */
	l.or	r3, r4, r0
	l.or	r4, r6, r0
#else
/* LINTSTUB: Func: void *memmove(void *, const void *, size_t) */
ENTRY(memmove)
#endif

	l.or	r11, r3, r0		/* Save dst (return value)	*/
	
	l.sfges	r4, r3			/* Branch to reverse if 	*/
	l.bnf	.Lreverse		/* src < dest. Don't want to	*/
					/* overwrite end of src with	*/
					/* start of dest 		*/

	l.addi	r4, r4, -4		/* Back up src and dst pointers */
	l.addi	r3, r3, -4		/* due to auto-update of 'load' */ 
	
	l.srli	r13, r5, 2		/* How many words in total cnt	*/
	l.sfeqi	r13, 0
	l.bf	.Llast1			/* Handle byte by byte if < 4	*/
					/* bytes total 			*/
	l.lwz	r7, 4(r4)		/* Preload first word		*/
	l.addi	r4, r4, 4
	
	l.j	.Lg1
	l.nop

.Lg0:					/* Main loop			*/

	l.lwz	r7, 4(r4)		/* Load a new word		*/
	l.sw	4(r3), r6		/* Store previous word		*/
	l.addi	r4, r4, 4		/* advance */
	l.addi	r3, r3, 4		/* advance */
	
.Lg1:

	l.addi	r13, r13, -1
	l.sfeqi	r13, 0
	l.bf	.Llast			/* Dec cnt, and branch if just	*/
	l.nop
					/* one word to store		*/
	l.lwz	r6, 4(r4)		/* Load another word		*/
	l.sw	4(r3), r7		/* Store previous word		*/
	l.addi	r4, r4, 4		/* advance to next word		*/
	l.addi	r3, r3, 4		/* advance to next word		*/
	l.addi	r13, r13, -1		/* Decrement count		*/
	l.sfeqi	r13, 0			/* last word?			*/
	l.bnf	.Lg0			/*    no, loop, more words	*/
	l.nop

	l.or	r7, r6, r0		/* If word count -> 0, then...	*/
	
.Llast:

	l.sw	4(r3), r7		/* ... store last word		*/
	l.addi	r3, r3, 4

.Llast1:				/* Byte-by-byte copy		*/

	l.andi	r5, r5, 3		/* get remaining byte count	*/
	l.sfeqi	r5, 0			/* is it 0?			*/
	l.bf	.Ldone			/*   yes, we're done		*/
	l.nop				/* -- delay slot --		*/

	l.lbz	r6, 4(r4)		/* 1st byte: update addr by 4	*/
	l.sb	4(r3), r6		/* since we pre-adjusted by 4	*/
	l.addi	r4, r4, 4		/* advance to next word		*/
	l.addi	r3, r3, 4		/* advance to next word		*/
	l.addi	r5, r5, -1		/* decrement count		*/
	l.sfeqi	r5, 0			/* is it 0?			*/
	l.bf	.Ldone			/*    yes, we're done		*/
	l.nop				/* -- delay slot --		*/

.Llast2:

	l.lbz	r6, 1(r4)		/* But handle the rest by	*/
	l.sb	1(r3), r6		/* updating addr by 1		*/
	l.addi	r4, r4, 1		/* advance to next word		*/
	l.addi	r3, r3, 1		/* advance to next word		*/
	l.addi	r5, r5, -1		/* decrement count		*/
	l.sfeqi	r5, 0			/* is it 0?			*/
	l.bnf	.Llast2			/*    yes, we're done		*/
	l.nop				/* -- delay slot --		*/
.Ldone:
	l.jr	lr			/* return			*/
	l.nop				/* -- delay slot --		*/

	/* We're here since src < dest. Don't want to overwrite end of	*/
	/* src with start of dest						*/

.Lreverse:

	l.add	r4, r4, r5		/* Work from end to beginning	*/
	l.add	r3, r3, r5 		/* so add count to string ptrs	*/
	l.srli	r13, r5, 2		/* Words in total count		*/
	l.sfeqi	r13, 0
	l.bf	.Lrlast1		/* Handle byte by byte if < 4	*/
					/* bytes total 			*/
	l.nop
	
	l.lwz	r7, -4(r4)		/* Preload first word		*/
	l.addi	r4, r4, -4		/* update pointer		*/

	l.j	.Lrg1

.Lrg0:					/* Main loop			*/

	l.lwz	r7, -4(r4)		/* Load a new word		*/
	l.sw	-4(r3), r6		/* Store previous word		*/
	l.addi	r4, r4, -4
	l.addi	r3, r3, -4
	
.Lrg1:

	l.addi	r13, r13, -1		/* decrement count		*/
	l.sfeqi	r13, 0			/* just one pending word left?	*/
	l.bf	.Lrlast			/*    yes, deal with it		*/

	l.lwz	r6, -4(r4)		/* Load another word		*/
	l.sw	-4(r3), r7		/* Store previous word		*/
	l.addi	r4, r4, -4
	l.addi	r3, r3, -4

	l.addi	r13, r13, -1		/* decrement count		*/
	l.sfeqi	r13, 0			/* just one pending word left?	*/
	l.bnf	.Lrg0			/*    no, loop again more words	*/
	l.nop

	l.or	r7, r6, r0		/* If word count -> 0, then...	*/
	
.Lrlast:

	l.sw	-4(r3), r7		/* ... store last word		*/
	l.addi	r3, r3, -4		/* update pointer */

.Lrlast1:				/* Byte-by-byte copy		*/

	l.andi	r5, r5, 3
	l.sfeqi	r5, 0
	l.bf	.Lrdone

.Lrlast2:

	l.lbz	r6, -1(r4)		/* Handle the rest, byte by 	*/
	l.sb	-1(r3), r6		/* byte				*/
	l.addi	r4, r4, -1
	l.addi	r3, r3, -1
	l.addi	r5, r5, -1		/* decrement count		*/
	l.sfeqi	r5, 0			/* is it 0?			*/
	l.bnf	.Lrlast2		/*    no, loop again		*/
	l.nop
.Lrdone:
	l.jr	lr
	l.nop

#ifdef _BCOPY
END(bcopy)
#else
END(memmove)
#endif
