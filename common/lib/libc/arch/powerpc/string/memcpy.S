/* $NetBSD: memcpy.S,v 1.6 2014/03/04 17:05:14 macallan Exp $ */

/* stropt/memcpy_440.S, pl_string_common, pl_linux 10/11/04 11:45:36
 * ==========================================================================
 * Optimized memcpy implementation for IBM PowerPC 440.
 *
 *  Copyright (c) 2003, IBM Corporation
 *  All rights reserved.	       
 *				     
 *  Redistribution and use in source and binary forms, with or      
 *  without modification, are permitted provided that the following 
 *  conditions are met:					     
 *								  
 *    * Redistributions of source code must retain the above	
 *      copyright notice, this list of conditions and the following 
 *      disclaimer.						 
 *    * Redistributions in binary form must reproduce the above     
 *      copyright notice, this list of conditions and the following 
 *      disclaimer in the documentation and/or other materials      
 *      provided with the distribution.			     
 *    * Neither the name of IBM nor the names of its contributors   
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.	 
 *								  
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND	  
 *  CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,     
 *  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF	
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE	
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS   
 *  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, 
 *  OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,	    
 *  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR  
 *  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
 *  OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT	
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE   
 *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *									  
 * ==========================================================================
 *
 * Function: Copy n bytes of the source to the destination. Behavior is
 *	   undefined for objects that overlap.
 *
 *
 *	   void *memcpy(void * dest, const void * src, int n)
 *
 * Input:  r3 - destination address
 *	 r4 - source address
 *	 r5 - byte count
 * Output: r3 - destination address
 *
 * ==========================================================================
 */

#include <machine/asm.h>
#ifdef _KERNEL_OPT
#include "opt_ppcarch.h"
#endif

	.text
	.align 4
/* LINTSTUB: Func: void *memcpy(void *, const void *, size_t) */
ENTRY(memcpy)
	/*
	 * Check count passed in R5. If zero, return; otherwise continue.
	 */
	cmpwi	%r5,0
	beqlr-

#if defined(_KERNEL) && defined(PPC_OEA601)
	/*
	* 601 will generate alignment exceptions if operand crosses
	 * 4k page boundary, so do byte copy when exception handler
	 * not available.  Maybe want to have a different memcpy for 601
	 * that checks for page boundaries/word alignment...
	 */
	mfspr   %r6, 287		/* mfpvbr %r6 PVR = 287        */
	srwi    %r6, %r6, 0x10		/* get version field from PVR  */
	cmpwi   %r6, 0x1		/* 601 CPU = 0x0001            */
	bne     bnorm			/* skip byte-only unless 601   */

	or	%r6, %r3, %r4		/* see if both source and dest */
	andi.	%r6, %r6, 3		/* are 32bit aligned           */
	beq	bnorm			/* skip byte-only if they are  */
bcpy:
	mtctr   %r5			/* byte copy everything */
	li      %r6, 0
bloop:		
	lbzx    %r7, %r4, %r6
	stbx    %r7, %r3, %r6
	addi    %r6, %r6, 1
	bdnz    bloop
	blr

bnorm:

#endif	

	mr	%r8, %r3		/* Copy dst (return value)	*/

	addi	%r4, %r4, -4		/* Prepare for main loop's auto	*/
	addi	%r8, %r8, -4		/* update		       */

	srwi.	%r9,%r5,2		/* Word count -> r9 		*/
	beq-	last1			/* Partial copy if <4 bytes	*/

	mtctr	%r9			/* Word cnt in CTR for loop     */
	lwzu	%r7, 4(%r4)		/* Preload for main loop	*/

	b	g1

g0:					/* Main loop			*/

	lwzu	%r7, 4(%r4)		/* Load a new word		*/
	stwu	%r6, 4(%r8)		/* Store previous word		*/

g1:

	bdz-	last			/* Dec ctr and exit loop if no  */
					/* more words		   */
	lwzu	%r6, 4(%r4)		/* Load another word		*/
	stwu	%r7, 4(%r8)		/* Store previous word		*/
	bdnz+	g0			/* Dec ctr and continue loop if */
					/* more words		   */

	mr	%r7, %r6

last:

	stwu	%r7, 4(%r8)		/* Store last word		*/

last1:					/* Byte-by-byte copy		*/

	clrlwi.	%r5,%r5,30
	beqlr

	mtctr	%r5

	lbzu	%r6, 4(%r4)		/* 1st byte: update by word	*/
	stbu	%r6, 4(%r8)
	bdzlr-

last2:

	lbzu	%r6, 1(%r4)		/* Handle the rest		*/
	stbu	%r6, 1(%r8)
	bdnz+	last2

	blr
END(memcpy)
