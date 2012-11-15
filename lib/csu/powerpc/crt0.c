/* $NetBSD: crt0.c,v 1.30 2011/03/12 07:56:36 matt Exp $ */

/*
 * Copyright (c) 1997 Jason R. Thorpe.
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
 * Small Data Area designators.  If not defined, will show up as being
 * at address zero.
 */
__weakref_visible int rtld_SDA_BASE_[] __weak_reference(_SDA_BASE_);
__weakref_visible int rtld_SDA2_BASE_[] __weak_reference(_SDA2_BASE_);

/*
 * First 5 arguments are specified by the PowerPC SVR4 ABI.  The
 * last argument, ps_strings, is a NetBSD extension.
 */
void _start(int, char **, char **, const Obj_Entry *,
		void (*)(void), struct ps_strings *);

void
_start(int argc, char **argv, char **envp,
    const Obj_Entry *obj,			/* from shared loader */
    void (*cleanup)(void),			/* from shared loader */
    struct ps_strings *ps_strings)		/* NetBSD extension */
{
	char *namep;

	/*
	 * Initialize the Small Data Area registers.
	 * _SDA_BASE is defined in the SVR4 ABI for PPC.
	 *
	 * Do the initialization in a PIC manner.
	 */
	__asm(
		"bcl 20,31,1f;"
		"1: mflr 11;"
		"addis 13,11,rtld_SDA_BASE_-1b@ha;"
		"addi 13,13,rtld_SDA_BASE_-1b@l;"
	    ::: "lr" );

	if ((namep = argv[0]) != NULL) {	/* NULL ptr if argc = 0 */
		if ((__progname = _strrchr(namep, '/')) == NULL)
			__progname = namep;
		else
			__progname++;
	}

	environ = envp;

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

	atexit(_fini);
	_init();

	exit(main(argc, argv, environ));
}

/*
 * NOTE: Leave the RCS ID _after_ __start(), in case it gets placed in .text.
 */
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: crt0.c,v 1.30 2011/03/12 07:56:36 matt Exp $");
#endif /* LIBC_SCCS and not lint */

#include "common.c"
