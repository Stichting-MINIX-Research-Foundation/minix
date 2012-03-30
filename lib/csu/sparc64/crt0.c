/* $NetBSD: crt0.c,v 1.24 2005/12/24 21:38:40 perry Exp $ */

/*
 * Copyright (c) 1995 Christopher G. Demetriou
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *          This product includes software developed for the
 *          NetBSD Project.  See http://www.NetBSD.org/ for
 *          information about NetBSD.
 * 4. The name of the author may not be used to endorse or promote products
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
 * 
 * <<Id: LICENSE,v 1.2 2000/06/14 15:57:33 cgd Exp>>
 */

#include "common.h"

/*
 * __start needs to gather up argc, argv, env_p, ps_strings, the termination
 * routine passed in %g1 and call ___start to finish up the startup processing.
 *
 * NB:	We are violating the ELF spec by passing a pointer to the ps strings in
 * 	%g1 instead of a termination routine.
 */

__asm("\n\
	.data\n\
__data_start:					! Start of data section\n\
	.text\n\
	.align 4\n\
	.global _start\n\
	.global __start\n\
	.register %g3,#scratch\n\
	.register %g2,#scratch\n\
_start:\n\
__start:\n\
	setx	__data_start, %o0, %g4		! Point %g4 to start of data section\n\
	clr	%g4				! egcs thinks this is zero. XXX\n\
	clr	%fp\n\
	add	%sp, 8*16 + 0x7ff, %o0		! start of stack\n\
	mov	%g1, %o1			! Cleanup routine\n\
	mov	%g3, %o1			! XXXX our rtld uses %g3\n\
	mov	%g2, %o2			! XXXX obj from rtld.\n\
	ba,pt	%icc, ___start			! XXXX jump over the retl egcs 2.96 inserts\n\
	mov	%g1, %o3			! ps_strings XXXX\n\
");

void ___start(char **, void (*cleanup)(void), const Obj_Entry *,
		struct ps_strings *);

void
___start(char **sp,
    void (*cleanup)(void),			/* from shared loader */
    const Obj_Entry *obj,			/* from shared loader */
    struct ps_strings *ps_strings)
{
	long argc;
	char **argv, *namep;

	argc = *(long *)sp;
	argv = sp + 1;
	environ = sp + 2 + argc;		/* 2: argc + NULL ending argv */

	if ((namep = argv[0]) != NULL) {	/* NULL ptr if argc = 0 */
		if ((__progname = _strrchr(namep, '/')) == NULL)
			__progname = namep;
		else
			__progname++;
	}

	if (ps_strings != (struct ps_strings *)0 &&
	    ps_strings != (struct ps_strings *)0xbabefacedeadbeef)
		__ps_strings = ps_strings;

#ifdef DYNAMIC
	if (&_DYNAMIC != NULL)
		_rtld_setup(cleanup, obj);
#endif

#ifdef MCRT0
	atexit(_mcleanup);
	monstartup((u_long)&_eprol, (u_long)&_etext);
#endif

	atexit(_fini);
	_init();

	exit(main(argc, argv, environ));
}

/*
 * NOTE: Leave the RCS ID _after_ _start(), in case it gets placed in .text.
 */
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: crt0.c,v 1.24 2005/12/24 21:38:40 perry Exp $");
#endif /* LIBC_SCCS and not lint */

#include "common.c"
