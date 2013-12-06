/* $NetBSD: __clone.S,v 1.9 2013/11/30 20:20:42 joerg Exp $ */

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

#include "SYS.h"
#include <sys/errno.h>

#ifdef WEAK_ALIAS
WEAK_ALIAS(clone, __clone)
#endif

/*
 * int __clone(int (*fn)(void *), void *stack, int flags, void *arg);
 */
ENTRY(__clone)

	/* test stack and function are not null */
#if defined(__thumb__) && defined(_ARM_ARCH_T2)
	cbz	r0, .Leinval
	cbz	r1, .Leinval
#elif !defined(__thumb__)
	cmp	r0, #0x00
	cmpne	r1, #0x00
	beq	.Leinval
#else
	cmp	r0, #0x00
	beq	.Leinval
	cmp	r1, #0x00
	beq	.Leinval
#endif

	/* place the func and its arg onto the child's stack */
#if !defined(__thumb__) || defined(_ARM_ARCH_T2)
	stmfd	r1!, {r0, r3} 
#else
	subs	r1, r1, #8
	stmia	r1!, {r0, r3} 
#endif
	
	/* syscall expects (flags, stack) */
	mov	r0, r2

	SYSTRAP(__clone)
#if !defined(__thumb__) || defined(_ARM_ARCH_T2)
	_INVOKE_CERROR()
#else
	bcs	.Lcerror
#endif

	/*
	 * r1 and r0 are the same as from fork:
	 * r1 == 0 in parent process, r1 == 1 in child process.
	 * r0 == pid of child in parent, r0 == pid of parent in child.
	 */
	/* if this is the parent then just return the pid */
#if defined(__thumb__)
#if defined(_ARM_ARCH_T2)
	cbz	r1, 1f
#else
	cmp	r1, #0x00
	bne	1f
#endif
	RET
1:
#else
	cmp	r1, #0x00
	RETc(eq)
#endif

	/*
	 * This is the child
	 * load the function and arg off the stack
	 */
	pop	{r1, r2}

	/* place arg in r0 */
	mov	r0, r2

	/* call the function */
#ifdef _ARM_ARCH_5T
	blx	r1
#else
	/* setup return address */
	mov	lr, pc
	RETr(r1)
#endif
	/* call _exit with the returned value */
	bl	PLT_SYM(_C_LABEL(_exit))

	/* NOTREACHED */

	/* error handler if func or stack is NULL */
	.align	0
.Leinval:
	movs	r0, #EINVAL
#if !defined(__thumb__) || defined(_ARM_ARCH_T2)
	b	CERROR
#else
.Lcerror:
	push	{lr}
	bl	CERROR
	pop	{pc}
#endif
END(__clone)
