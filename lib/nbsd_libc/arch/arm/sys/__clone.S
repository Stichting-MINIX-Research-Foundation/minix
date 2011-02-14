/* $NetBSD: __clone.S,v 1.6 2006/04/24 19:00:30 snj Exp $ */

/*
 * Copyright (c) 2001 Christopher Gilbert
 * All rights reserved.
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <machine/asm.h>
#include <sys/errno.h>
#include "SYS.h"

#ifdef WEAK_ALIAS
WEAK_ALIAS(clone, __clone)
#endif

/*
 * int __clone(int (*fn)(void *), void *stack, int flags, void *arg);
 */
ENTRY(__clone)

	/* test stack and function are not null */
	teq	r0, #0x00
	teqne	r1, #0x00
	beq	.Leinval

	/* place the func and its arg onto the child's stack */
	stmfd	r1!, {r0, r3} 
	
	/* syscall expects (flags, stack) */
	mov	r0, r2

	SYSTRAP(__clone)
	bcs	PIC_SYM(CERROR, PLT)

	/*
	 * r1 and r0 are the same as from fork:
	 * r1 == 0 in parent process, r1 == 1 in child process.
	 * r0 == pid of child in parent, r0 == pid of parent in child.
	 */
	teq	r1, #0x00

	/* if this is the parent then just return the pid */
	RETc(eq)
	/*
	 * This is the child
	 * load the function and arg off the stack
	 */
	ldmfd	sp!, {r1, r2}

	/* setup return address */
	add	lr, pc, #.Lreturnhere - . - 8

	/* place arg in r0 */
	mov	r0, r2

	/* call the function */
#ifdef _ARM_ARCH_4T
	bx	r1
#else
	mov	pc, r1
#endif

.Lreturnhere:
	/* call _exit with the returned value */
	b	PIC_SYM(_C_LABEL(_exit), PLT)

	/* NOTREACHED */

	/* error handler if func or stack is NULL */
.Leinval:
	mov	R0, #EINVAL
	b	PIC_SYM(CERROR, PLT)
