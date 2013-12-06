/*	$NetBSD: udivsi3.S,v 1.4 2013/07/16 23:24:18 matt Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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

#include <machine/asm.h>

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
	RCSID("from: @(#)udivsi3.s	5.1 (Berkeley) 6/7/90")
#else
	RCSID("$NetBSD: udivsi3.S,v 1.4 2013/07/16 23:24:18 matt Exp $")
#endif
#endif /* LIBC_SCCS and not lint */

/* unsigned / unsigned */
#ifndef __mc68010__
ENTRY(__udivsi3)
	movel	4(%sp),%d0
	divul	8(%sp),%d0
	rts
END(__udivsi3)
#else
ENTRY(__udivsi3)
	movel	%d2, -(%sp)	| save %d2
	movel	12(%sp), %d0	| load divisor
	movel	8(%sp), %d1	| load dividend

| first, we divide the divisor and dividend by two until 
| the divisor fits into 16 bits:
1:	cmpil	#0x10000, %d0
	bcs	2f
	lsrl	#1, %d0
	lsrl	#1, %d1
	bra	1b
2:

| now we can do the divide.  to avoid overflow, we have to 
| do the divide in two parts, high and low, and add the 
| results together:
	movew	%d1, %d2	| save low(dividend)
	clrw	%d1
	swap	%d1		| %d1 = dividend >> 16
	divuw	%d0, %d1	| do the high divide
	moveal	%d1, %a1	| save high divide result
	movew	%d2, %d1	| concat(remainder, low(dividend))
	divuw	%d0, %d1	| do the low divide
	movel	%a1, %d0	| recover high divide result
	swap	%d0
	clrw	%d0		| %d0 = finished high divide result
	andil	#0xffff, %d1	| %d1 = finished low divide result
	addl	%d1, %d0	| %d0 = quotient guess

| the quotient we have so far is only a guess.  the divide we 
| did above was really the divide of some dividendB by some 
| divisorB, where the following hold:
|
| (dividend - divisor) <= dividendB <= dividend
| (divisor / 2) < divisorB <= divisor
|
| so our guess quotient cannot be less than our real desired
| quotient.  however, it might be one too big.
|
| to adjust this quotient, we multiply it by the original 
| divisor and subtract the result from the original dividend.  
| if the result is nonnegative, our guessed quotient was 
| correct, and the subtraction result is our remainder.  
| if the result is negative, our guessed quotient was one 
| too big, and the subtraction result plus the original 
| divisor is our remainder.
|
| as in mulsi3, we have to do the multiply in stages to avoid 
| overflow:

	movel	12(%sp), %d2	| load divisor
	swap	%d2
	movel	%d0, %d1
	muluw	%d2, %d1	| high(divisor) * low(guess)
	moveal	%d1, %a1	| save high(divisor) * low(guess)
	swap	%d2
	movel	%d0, %d1
	swap	%d1
	muluw	%d2, %d1	| low(divisor) * high(guess)
	addl	%a1, %d1
	swap	%d1
	clrw	%d1		| %d1 = finished high multiply result
	moveal	%d2, %a1	| save original divisor
	muluw	%d0, %d2	| low(guess) * low(divisor)
	addl	%d1, %d2	| %d2 = guess * divisor
	
	movel	8(%sp), %d1	| load original dividend
	subl	%d2, %d1	| subtract
	bcc	3f
	subql	#1, %d0		| adjust quotient
	addl	%a1, %d1	| adjust remainder
3:	movel	(%sp)+, %d2	| restore %d2
	rts
END(__udivsi3)
#endif /* __mc68010__ */
