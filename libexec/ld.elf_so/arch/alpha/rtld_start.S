/*	$NetBSD: rtld_start.S,v 1.16 2004/02/18 23:04:49 enami Exp $	*/

/*
 * Copyright 1996 Matt Thomas <matt@3am-software.com>
 * Portions copyright 2002 Charles M. Hannum <root@ihack.net>
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

/*
 * Note: we can call ourselves LEAF even though we use callee-saved
 * registers because we're the root of the call graph.
 */
LEAF_NOPROFILE(_rtld_start, 0)
	.set	noreorder
	br	pv, 1f
1:	LDGP(pv)

	/*
	 * Relocate ourself.
	 */
	br	s2, 2f		/* get our PC */
2:	ldiq	s3, 2b		/* get where the linker thought we were */

	subq	s2, s3, a1	/* relocbase */
	lda	t5, _DYNAMIC
	addq	a1, t5, a0	/* &_DYNAMIC */

	/* Squirrel away ps_strings. */
	mov	a3, s0

	bsr	ra, _rtld_relocate_nonplt_self
	LDGP(ra)

	/*
	 * Allocate space on the stack for the cleanup and obj_main
	 * entries that _rtld() will provide for us.
	 */
	lda	sp, -16(sp)

	subq	s2, s3, a1	/* relocbase */
	mov	sp, a0		/* sp */
	CALL(_rtld)		/* v0 = _rtld(sp, relocbase); */

	ldq	a1, 0(sp)	/* cleanup */
	ldq	a2, 8(sp)	/* obj_main */
	lda	sp, 16(sp)	/* pop stack */

	mov	sp, a0		/* stack pointer */
	mov	s0, a3		/* ps_strings */

	mov	v0, pv		/* set up PV for entry point */

	jsr	ra, (v0), 0	/* (*_start)(sp, cleanup, obj, ps_strings); */
	ldgp	gp, 0(ra)

	CALL(exit)
	halt
END(_rtld_start)

#define	RTLD_BIND_START_PROLOGUE					\
	/* at_reg already used by PLT code. */				\
	.set	noat						;	\
									\
	/*								\
	 * Allocate stack frame and preserve all registers that the	\
	 * caller would have normally saved themselves.			\
	 */								\
	lda	sp, -168(sp)					;	\
	stq	ra, 0(sp)					;	\
	stq	v0, 8(sp)					;	\
	stq	t0, 16(sp)					;	\
	stq	t1, 24(sp)					;	\
	stq	t2, 32(sp)					;	\
	stq	t3, 40(sp)					;	\
	stq	t4, 48(sp)					;	\
	stq	t5, 56(sp)					;	\
	stq	t6, 64(sp)					;	\
	stq	t7, 72(sp)					;	\
	stq	a0, 80(sp)					;	\
	stq	a1, 88(sp)					;	\
	stq	a2, 96(sp)					;	\
	stq	a3, 104(sp)					;	\
	stq	a4, 112(sp)					;	\
	stq	a5, 120(sp)					;	\
	stq	t8, 128(sp)					;	\
	stq	t9, 136(sp)					;	\
	stq	t10, 144(sp)					;	\
	stq	t11, 152(sp)					;	\
	stq	gp, 160(sp)					;	\
									\
	/*								\
	 * Load our global pointer.  Note, can't use pv, since it is	\
	 * already used by the PLT code.				\
	 */								\
	br	t0, 1f						;	\
1:	LDGP(t0)

#define	RTLD_BIND_START_EPILOGUE					\
	/* Move the destination address into position. */		\
	mov	v0, pv						;	\
									\
	/* Restore program registers. */				\
	ldq	ra, 0(sp)					;	\
	ldq	v0, 8(sp)					;	\
	ldq	t0, 16(sp)					;	\
	ldq	t1, 24(sp)					;	\
	ldq	t2, 32(sp)					;	\
	ldq	t3, 40(sp)					;	\
	ldq	t4, 48(sp)					;	\
	ldq	t5, 56(sp)					;	\
	ldq	t6, 64(sp)					;	\
	ldq	t7, 72(sp)					;	\
	ldq	a0, 80(sp)					;	\
	ldq	a1, 88(sp)					;	\
	ldq	a2, 96(sp)					;	\
	ldq	a3, 104(sp)					;	\
	ldq	a4, 112(sp)					;	\
	ldq	a5, 120(sp)					;	\
	ldq	t8, 128(sp)					;	\
	ldq	t9, 136(sp)					;	\
	ldq	t10, 144(sp)					;	\
	ldq	t11, 152(sp)					;	\
	ldq	gp, 160(sp)					;	\
	/* XXX LDGP? */							\
									\
	/*								\
	 * We've patched the PLT; sync the I-stream.			\
	 */								\
	imb							;	\
									\
	/* Pop the stack frame and turn control to the destination. */	\
	lda     sp, 168(sp)					;	\
	jmp	zero, (pv)

/*
 * Lazy binding entry point, called via PLT.
 */
NESTED_NOPROFILE(_rtld_bind_start, 0, 168, ra, 0, 0)

	RTLD_BIND_START_PROLOGUE

	/* Set up the arguments for _rtld_bind. */
	subq	at_reg, pv, a1		/* calculate offset of reloc entry */
	ldq	a0, 8(pv)		/* object structure */
	subq	a1, 20, a1		/* = (at - pv - 20) / 12 * 24 */
	addq	a1, a1, a1

	CALL(_rtld_bind)

	RTLD_BIND_START_EPILOGUE

END(_rtld_bind_start)

/*
 * Lazy binding entry point, called via PLT.  This version is for the
 * old PLT entry format.
 */
NESTED_NOPROFILE(_rtld_bind_start_old, 0, 168, ra, 0, 0)

	RTLD_BIND_START_PROLOGUE

	/* Set up the arguments for _rtld_bind. */
	ldq	a0, 8(pv)		/* object structure */
	mov	at_reg, a1		/* offset of reloc entry */

	CALL(_rtld_bind)

	RTLD_BIND_START_EPILOGUE

END(_rtld_bind_start_old)
