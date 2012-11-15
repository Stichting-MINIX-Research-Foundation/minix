/*	$NetBSD: crt0.c,v 1.10 2011/03/07 05:09:10 joerg Exp $	*/

/*
 * Copyright (c) 2002 Matt Fredette
 * Copyright (c) 1999 Klaus Klein
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

static void ___start(struct ps_strings *,
    void (*cleanup)(void), const Obj_Entry *, int)
#ifdef __GNUC__
    __attribute__((__used__))
#endif
    ;

__asm("\n"
"	.text				\n"
"	.align	4			\n"
"	.globl	_start			\n"
"	.globl	__start			\n"
"	.type	_start,@function	\n"
"	.type	__start,@function	\n"
"_start:				\n"
"__start:				\n"
"	.import	_GLOBAL_OFFSET_TABLE_	\n"
"\n"
"	bl      L$lpc, %r27		\n"
"	depi    0, 31, 2, %r27		\n"
"L$lpc:	addil   L'_GLOBAL_OFFSET_TABLE_ - ($PIC_pcrel$0 - 8), %r27	\n"
"	ldo     R'_GLOBAL_OFFSET_TABLE_ - ($PIC_pcrel$0 - 12)(%r1),%r27	\n"
"	copy	%r27, %r19		\n"
"	b	___start		\n"
"	copy	%r27, %arg3		\n");

static void
___start(struct ps_strings *ps_strings,
    void (*cleanup)(void),			/* from shared loader */
    const Obj_Entry *obj,			/* from shared loader */
    int dp)
{
	int argc;
	char **argv;
	int fini_plabel[2];

	argc = ps_strings->ps_nargvstr;
	argv = ps_strings->ps_argvstr;
	environ = ps_strings->ps_envstr;

	if ((__progname = argv[0]) != NULL) {	/* NULL ptr if argc = 0 */
		if ((__progname = _strrchr(__progname, '/')) == NULL)
			__progname = argv[0];
		else
			__progname++;
	}

	if (ps_strings != (struct ps_strings *)0)
		__ps_strings = ps_strings;

#ifdef DYNAMIC
	if (&rtld_DYNAMIC != NULL)
		_rtld_setup(cleanup, obj);
#endif

	_libc_init();

#ifdef MCRT0
	atexit(_mcleanup);
	monstartup((u_long)&_eprol, (u_long)&_etext);
#endif

	/*
	 * Since crt0.o, crtbegin.o, and crtend.o are always
	 * compiled PIC, they must have %r19 set correctly on
	 * entry to any function they contain.  However, when
	 * a program is linked statically, the linker does
	 * not fill a PLABEL relocation with a pointer to a 
	 * true PLABEL, it just fills it with the offset of the
	 * function.  This shows the linker's assumption that 
	 * when linking statically, *all* of the code has *not* 
	 * been compiled PIC.  I guess to assume otherwise
	 * would be a performance hit, as you would end up
	 * with unnecessary PLABELs for function pointers.
	 *
	 * But here, passing the address of the PIC _fini to
	 * atexit, we must make sure that we pass a PLABEL.
	 */
	fini_plabel[0] = (int)_fini;
	if (fini_plabel[0] & 2)
		/* _fini is already a PLABEL. */
		atexit(_fini);
	else {
		fini_plabel[1] = dp;
		atexit((void (*)(void))(((int)fini_plabel) | 2));
	}
	_init();

	exit(main(argc, argv, environ));
}

/*
 * NOTE: Leave the RCS ID _after_ __start(), in case it gets placed in .text.
 */
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: crt0.c,v 1.10 2011/03/07 05:09:10 joerg Exp $");
#endif /* LIBC_SCCS and not lint */

#include "common.c"
