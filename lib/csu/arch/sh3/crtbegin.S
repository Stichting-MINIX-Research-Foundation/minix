/*	$NetBSD: crtbegin.S,v 1.3 2013/09/12 15:36:14 joerg Exp $	*/
/*-
 * Copyright (c) 2012 Valeriy E. Ushakov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <machine/asm.h>

RCSID("$NetBSD: crtbegin.S,v 1.3 2013/09/12 15:36:14 joerg Exp $")

	.section	.ctors, "aw", @progbits
	.p2align 2
__CTOR_LIST__:
	.long -1

	.section	.dtors, "aw", @progbits
	.p2align 2
__DTOR_LIST__:
	.long -1

	.section	.eh_frame, "a", @progbits
	.p2align 2
__EH_FRAME_LIST__:

	.section	.jcr, "aw", @progbits
	.p2align 2
__JCR_LIST__:

	.section	.data.rel, "aw", @progbits
	.p2align 2
	.globl	__dso_handle
	.hidden	__dso_handle
	.type	__dso_handle, @object
	.size	__dso_handle, 4
__dso_handle:
#ifdef SHARED
	.long	__dso_handle
#else
	.long	0
#endif

__dwarf_eh_object:
	.zero	32

__initialized:
	.zero	1
__finished:
	.zero	1

	.text
	.weak	__cxa_finalize
	.weak	__deregister_frame_info
	.weak	__register_frame_info
	.weak	_Jv_RegisterClasses

/*
 * A bit of CPP syntactic sugar for accessing variables.
 *
 * For PIC we are obliged to use @(r0, r12) since r12 has the GOT
 * address and only r0 can be used in @(r0, Rm) addressing mode, so we
 * always load variable address to r0.
 */
#ifdef __PIC__
#define VAR_DATUM(var)	var@GOTOFF
#define FUNC_DATUM(f)	f@GOT
#define R0VAR		(r0, r12)
#else
#define VAR_DATUM(var)	var
#define FUNC_DATUM(f)	f
#define R0VAR		r0
#endif


__do_global_ctors_aux:
        mov.l   r8, @-sp
        mov.l   r9, @-sp
#ifdef __PIC__
        mov.l   r12, @-sp
        mov.l   .Lc_got, r12
        mova    .Lc_got, r0
        add     r0, r12
#endif
        mov.l   r14, @-sp
        sts.l   pr, @-sp
	mov     sp, r14

	!! if (__initialized) return;
	mov.l	.Lc___initialized, r0
	mov.b	@R0VAR, r1
	tst	r1, r1
	bf	.Lc_return

	!! __initialized = 1;
	mov	#1, r1
	mov.b	r1, @R0VAR


	!! if (__register_frame_info)
	!!     __register_frame_info(&__EH_FRAME_LIST__[0], &__dwarf_eh_object)
#ifdef __PIC__
	mov.l	.Lc___register_frame_info_GOT, r0
	mov.l	@R0VAR, r1
	tst	r1, r1
	bt	.Lc___register_frame_info_done
	mov.l	.Lc___register_frame_info, r0
	mov.l	.Lc___EH_FRAME_LIST__, r4
	mov.l	.Lc___dwarf_eh_object, r5
	add	r12, r4
.Lc___register_frame_info_call:
	CALL	r0
	 add	r12, r5
#else /* !PIC */
	mov.l	.Lc___register_frame_info, r0
	tst	r0, r0
	bt	.Lc___register_frame_info_done
	mov.l	.Lc___EH_FRAME_LIST__, r4
	mov.l	.Lc___dwarf_eh_object, r5
	CALL	r0
	 nop
#endif
.Lc___register_frame_info_done:

	!!  if (_Jv_RegisterClasses && __JCR_LIST__[0])
	!!      _Jv_RegisterClasses(&__JCR_LIST__[0]);
#ifdef __PIC__
	mov.l	.Lc__Jv_RegisterClasses_GOT, r0
	mov.l	@R0VAR, r1
	tst	r1, r1
	bt	.Lc__Jv_RegisterClasses_done

	mov.l	.Lc___JCR_LIST__, r0
	mov.l	@R0VAR, r1
	tst	r1, r1
	bt	.Lc__Jv_RegisterClasses_done

	mov.l	.Lc__Jv_RegisterClasses, r2
	mov	r0, r4
.Lc__Jv_RegisterClasses_call:
	CALL	r2
	 add	r12, r4

#else /* !PIC */
	mov.l	.Lc__Jv_RegisterClasses, r2
	tst	r2, r2
	bt	.Lc__Jv_RegisterClasses_done

	mov.l	.Lc___JCR_LIST__, r0
	mov.l	@R0VAR, r1
	tst	r1, r1
	bt	.Lc__Jv_RegisterClasses_done

	mov	r0, r4
.Lc__Jv_RegisterClasses_call:
	CALL	r2
	 add	r12, r4
#endif
.Lc__Jv_RegisterClasses_done:


	!! call all constructors on __CTOR_LIST__ in reverse order
	mov.l	.Lc___CTOR_LIST_END__, r8
#ifdef __PIC__
	add	r12, r8
#endif
	add	#-4, r8
	mov.l	@r8, r9
	not	r9, r0		! sentinel at __CTOR_LIST__[0] is -1
.Lc_ctor_list_loop:
	tst	r0, r0
	bt.s	.Lc_ctor_list_done
	 add	#-4, r8
	jsr	@r9
	 mov.l	@r8, r9
	bra	.Lc_ctor_list_loop
	 not	r9, r0
.Lc_ctor_list_done:

.Lc_return:
	mov	r14, sp
	lds.l	@sp+, pr
	mov.l	@sp+, r14
#ifdef __PIC__
	mov.l	@sp+, r12
#endif
	mov.l	@sp+, r9
	rts
	 mov.l	@sp+, r8

	.p2align 2
.Lc_got:
	PIC_GOT_DATUM
.Lc___initialized:
	.long	VAR_DATUM(__initialized)
#ifdef __PIC__
.Lc___register_frame_info_GOT:
	.long	__register_frame_info@GOT
#endif
.Lc___register_frame_info:
	CALL_DATUM(__register_frame_info, .Lc___register_frame_info_call)
.Lc___EH_FRAME_LIST__:
	.long	VAR_DATUM(__EH_FRAME_LIST__)
.Lc___dwarf_eh_object:
	.long	VAR_DATUM(__dwarf_eh_object)
#ifdef __PIC__
.Lc__Jv_RegisterClasses_GOT:
	.long	_Jv_RegisterClasses@GOT
#endif
.Lc__Jv_RegisterClasses:
	CALL_DATUM(_Jv_RegisterClasses, .Lc__Jv_RegisterClasses_call)
.Lc___JCR_LIST__:
	.long	VAR_DATUM(__JCR_LIST__)
.Lc___CTOR_LIST_END__:
	.long	VAR_DATUM(__CTOR_LIST_END__)


__do_global_dtors_aux:
        mov.l   r8, @-sp
        mov.l   r9, @-sp
#ifdef __PIC__
        mov.l   r12, @-sp
        mov.l   .Ld_got, r12
        mova    .Ld_got, r0
        add     r0, r12
#endif
        mov.l   r14, @-sp
        sts.l   pr, @-sp
	mov     sp, r14

	!! if (__finished) return;
	mov.l	.Ld___finished, r0
	mov.b	@R0VAR, r1
	tst	r1, r1
	bf	.Ld_return

	!! __finished = 1;
	mov	#1, r1
	mov.b	r1, @R0VAR

#ifdef SHARED /* implies PIC */
	!! if (__cxa_finalize)
	!!     __cxa_finalize(&__dso_handle);
	mov.l	.Ld___cxa_finalize_GOT, r0
	mov.l	@R0VAR, r1
	tst	r1, r1
	bt	.Ld___cxa_finalize_done
	mov.l	.Ld___cxa_finalize, r0
	mov.l	.Ld___dso_handle, r4
.Ld___cxa_finalize_call:
	CALL	r0
	 add	r12, r4
.Ld___cxa_finalize_done:
#endif	/* SHARED */

	!! call all destructors on __DTOR_LIST__
	mov.l	.Ld___DTOR_LIST__, r8
#ifdef __PIC__
	add	r12, r8
#endif
	add	#4, r8		! skip first entry that we know to be -1
	mov.l	@r8+, r9
	tst	r9, r9
.Ld_dtor_list_loop:
	bt	.Ld_dtor_list_done
	jsr	@r9
	 mov.l	@r8+, r9
	bra	.Ld_dtor_list_loop
	 tst	r9, r9
.Ld_dtor_list_done:

	!! if (__deregister_frame_info)
	!!     __deregister_frame_info(&__EH_FRAME_LIST__[0]);
#ifdef __PIC__
	mov.l	.Ld___deregister_frame_info_GOT, r0
	mov.l	@R0VAR, r1
	tst	r1, r1
	bt	.Ld___deregister_frame_info_done
	mov.l	.Ld___deregister_frame_info, r0
	mov.l	.Ld___EH_FRAME_LIST__, r4
.Ld___deregister_frame_info_call:
	CALL	r0
	 add	r12, r4
#else /* !PIC */
	mov.l	.Ld___deregister_frame_info, r0
	tst	r0, r0
	bt	.Ld___deregister_frame_info_done
	mov.l	.Ld___EH_FRAME_LIST__, r4
	CALL	r0
	 nop
#endif
.Ld___deregister_frame_info_done:

.Ld_return:
	mov	r14, sp
	lds.l	@sp+, pr
	mov.l	@sp+, r14
#ifdef __PIC__
	mov.l	@sp+, r12
#endif
	mov.l	@sp+, r9
	rts
	 mov.l	@sp+, r8

	.p2align 2
.Ld_got:
	PIC_GOT_DATUM
.Ld___finished:
	.long	VAR_DATUM(__finished)
#ifdef SHARED /* implies PIC */
.Ld___cxa_finalize_GOT:
	.long	__cxa_finalize@GOT
.Ld___cxa_finalize:
	CALL_DATUM(__cxa_finalize, .Ld___cxa_finalize_call)
.Ld___dso_handle:
	.long	VAR_DATUM(__dso_handle)
#endif
.Ld___DTOR_LIST__:
	.long	VAR_DATUM(__DTOR_LIST__)
#ifdef __PIC__
.Ld___deregister_frame_info_GOT:
	.long	__deregister_frame_info@GOT
#endif
.Ld___deregister_frame_info:
	CALL_DATUM(__deregister_frame_info, .Ld___deregister_frame_info_call)
.Ld___EH_FRAME_LIST__:
	.long	VAR_DATUM(__EH_FRAME_LIST__)



#define _CALL_INIT_FINI_FUNCTION(func)					\
	mov.l	1f, r1;							\
	mova	2f, r0;							\
0:	braf	r1;	/* NB: branch, not call ... */			\
	 lds	r0, pr;	/* skip the following .long when returning */	\
	.p2align 2;							\
1:	.long func - (0b+4);						\
2:	;

	.section	.init, "ax", @progbits
	_CALL_INIT_FINI_FUNCTION(__do_global_ctors_aux)

	.section	.fini, "ax", @progbits
	_CALL_INIT_FINI_FUNCTION(__do_global_dtors_aux)
