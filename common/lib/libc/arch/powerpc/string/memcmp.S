/* $NetBSD: memcmp.S,v 1.3 2011/01/15 07:31:12 matt Exp $ */

/* stropt/memcmp.S, pl_string_common, pl_linux 10/11/04 11:45:35
 * ==========================================================================
 * Optimized memcmp implementation for IBM PowerPC 405/440.
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
 * Function: Compare two character strings (up to n characters)
 *
 *		int memcmp(const char *s1, const char *s2, int n)
 *
 * Input:	r3 - buffer 1 address
 *	 	r4 - buffer 2 address
 *	 	r5 - maximum characters to compare
 * Output: r3 <0 (less), 0 (equal), >0 (greater)
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
	cmpwi	%r5,0
	beq-	ret_0;

	/*
	 * Most of the time the difference is found in the first
	 * several bytes.  The following code minimizes the number
	 * of load operations for short compares.
	 */

	mr	%r11, %r3		/* Save buffer 1		*/

again:

	not	%r10, %r4		/* buffer 2: bytes to page bdy	*/
	rlwinm.	%r10, %r10,29,23,31	/* buffer 2: dwords to page bdy	*/
	beq-	bytebybyte		/* If < 8 bytes to the page bdy */
					/* do byte by byte		*/
	lwz	%r8, 0(%r4)		/* load 1st buffer 2 word	*/

	not	%r12, %r11		/* buffer 1: bytes to page bdy	*/
	rlwinm.	%r12, %r12,29,23,31	/* buffer 1: dwords to page bdy	*/
	beq-	bytebybyte		/* If < 8 bytes to the page bdy */
					/* do byte by byte		*/
	lwz	%r6, 0(%r11)		/* load 1st buffer 1 word	*/

	cmpwi	%r5, 4			/* If remaining count <= 4	*/
	ble+	first4			/* handle specially.	DWG	*/

	cmplw	%r8, %r6		/* compare buffer 2 and buffer 1*/
	bne+	all_done		/* different => we're done	*/

	lwzu	%r9, 4(%r4)		/* load 2nd buffer 2 word	*/
	lwzu	%r7, 4(%r11)		/* load 2nd buffer 1 word	*/

	cmpwi	%r5, 8			/* If remaining count <= 8	*/
	ble+	last4			/* handle specially.	DWG	*/

	cmplw	%r9, %r7		/* compare buffer 2 and buffer 1*/
	bne+	all_done		/* different => we're done	*/

	addi	%r5, %r5, -8		/* Update character counter DWG */
	addi	%r10, %r4, 0x0004	/* DWG*/
	not	%r10, %r10		/* buffer 2: bytes to page bdy DWG */
	rlwinm.	%r10, %r10,29,23,31	/* buffer 2: dwords to page bdy	DWG */
	addi	%r12, %r11, 0x0004	/* DWG */
	not	%r12, %r12		/* buffer 1: bytes to page bdy DWG */
	rlwinm.	%r12, %r12,29,23,31	/* buffer 1: dwords to page bdy	DWG */
	
	/* The following section prior to loop: figures out whether	*/
	/* the buffer 1 or buffer 2 is closer to the page boundary.	*/
	/* The main loop count is then set up to reflect the number of	*/
	/* double words of the buffer that is closest			*/

	cmpw	%r10, %r12		/* Find closest			*/
	blt	lt

	mr	%r10, %r12

lt:

	srwi	%r12, %r5, 3		/* Double check the total count */
	cmpw	%r10, %r12		/* limitation			*/
	blt	lt2

	mr	%r10, %r12		/* DWG */
lt2:					/* DWG */
	cmpwi	%r10, 0			/* DWG */
	bne	lt3			/* DWG */
	addi	%r4, %r4, 0x0004	/* DWG */
	addi	%r11,%r11,0x0004	/* DWG */
	b	again			/* DWG */
lt3:					/* DWG */
	mtctr	%r10			/* dword count for loop		*/
	lwzu	%r6, 4(%r11)		/* pre-load buffer 1 word	*/

	b	in			/* To the loop			*/

loop:					/* main loop			*/

	cmplw	%r8, %r6		/* Compare first buffer 2 word	*/
	bne-	all_done		/* with first buffer 1 word	*/
					/* If different, we're done	*/
	cmplw	%r9, %r7		/* Compare second buffer 2 word	*/
					/* with second buffer 1 word	*/
	lwzu	%r6, 4(%r11)		/* pre-load buffer 1 word	*/

	bne-	all_done		/* If different, we're done	*/

in:

	lwzu	%r7, 4(%r11)		/* pre-load buffer 1 word	*/
	lwzu	%r8, 4(%r4)		/* pre-load buffer 2 word	*/
	lwzu	%r9, 4(%r4)		/* pre-load buffer 2 word	*/

	bdnz+	loop			/* Do more DW's if cnt > 0	*/

	/*mfctr	%r12*/ /*DWG*/		/* number of dwords left	*/
	/*subf	%r10, %r12, %r10*/ /*DWG*//* number of dwords compared	*/
	slwi	%r10, %r10, 3
	subf	%r5, %r10, %r5		/* adjust byte counter		*/
	/*bne+	partial*/ /*DWG*/	/* If less than 8 bytes, handle */
					/* specially			*/
	/*cmpwi	%r5, 8*/		/* Removed.		 DWG */
	/*blt	partial*/		/* Removed.		 DWG */

	/*addic	%r5, %r5, -8*/ /*DWG*/	/* Subtract two words from count*/

	cmplw	%r8, %r6		/* compare last dword		*/
	addi	%r4, %r4, 4
	bne-	all_done

	cmplw	%r9, %r7
	addi	%r11, %r11, 4
	bne-	all_done

bytebybyte:

	/* We've gotten close to a page boundary: do a byte-byte-byte
	 * compare for the following 8 bytes, and then go back to
	 * the full-word compare loop.
	 */

	li	%r3, 8			/* loop count			*/
	cmpw	%r3, %r5		/* take min(8, counter)		*/
	ble	f2

	mr.	%r3, %r5

	beqlr

f2:

	mtctr	%r3
	subf	%r5, %r3, %r5		/* adjust counter		*/

bbb:

	lbz	%r6, 0(%r11)		/* byte copy loop		*/

	addi	%r11, %r11, 1

	lbz	%r8, 0(%r4)

	addi	%r4, %r4, 1

	cmplw	%r8, %r6

	bdnzt+	eq, bbb

	bne	all_done

	cmpwi	%r5, 0
	bgt	again			/* handle the rest		*/

	xor	%r3,%r3,%r3

	blr

#if 0 /* Removed code section. DWG */
partial:

	mr.	%r3, %r5

	beqlr				/* If count -> 0, we're done	*/

f1:

	subfic	%r3, %r3, 4		/* zero/end in first word?	*/
	cmpwi	%r3, 0
	blt	last4
#endif /* DWG */

first4:
	subfic	%r3, %r5, 4		/* If count <= 4, handle 	*/
	rlwinm	%r3, %r3, 3, 0, 31	/* count *= 8			*/
	srw	%r6, %r6, %r3		/* align 1st buffer 1 word	*/
	srw	%r8, %r8, %r3		/* align 1st buffer 2 word	*/

	cmplw	%r8, %r6		/* get result			*/
	bne	all_done
	xor	%r3,%r3,%r3
	blr

last4:
	subfic	%r10, %r5, 8		/*DWG*/
	rlwinm	%r10, %r10, 3, 0, 31	/* count *= 8			*/
	srw	%r7, %r7, %r10		/* align 2nd buffer 1 word	*/
	srw	%r9, %r9, %r10		/* align 2nd buffer 2 word	*/

	cmplw	%r9, %r7		/* get result			*/
	bne	all_done
ret_0:
	xor	%r3,%r3,%r3		/* Equal result		 */
	blr

all_done:

	blt	finish_lt

	addi	%r3,0,-1		/* Less than result		*/

	blr

finish_lt:

	addi	%r3,0,1			/* Greater than result		*/

	blr
END(memcmp)
