/* $NetBSD: crt0-common.c,v 1.13 2013/01/31 22:24:25 matt Exp $ */

/*
 * Copyright (c) 1998 Christos Zoulas
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

#include <sys/cdefs.h>
__RCSID("$NetBSD: crt0-common.c,v 1.13 2013/01/31 22:24:25 matt Exp $");

#include <sys/types.h>
#include <sys/exec.h>
#if !defined(__minix)
#include <sys/syscall.h>
#else
#include <string.h>
#endif /* !defined(__minix) */
#include <machine/profile.h>
#include <stdlib.h>
#include <unistd.h>

#include "rtld.h"

extern int main(int, char **, char **);

#ifndef HAVE_INITFINI_ARRAY
extern void	_init(void);
extern void	_fini(void);
#endif
extern void	_libc_init(void);

/*
 * Arrange for _DYNAMIC to be weak and undefined (and therefore to show up
 * as being at address zero, unless something else defines it).  That way,
 * if we happen to be compiling without -static but with without any
 * shared libs present, things will still work.
 */

__weakref_visible int rtld_DYNAMIC __weak_reference(_DYNAMIC);

#ifdef MCRT0
extern void	monstartup(u_long, u_long);
extern void	_mcleanup(void);
extern unsigned char __etext, __eprol;
#endif /* MCRT0 */

char		**environ;
struct ps_strings *__ps_strings = 0;

static char	 empty_string[] = "";
char		*__progname = empty_string;

__dead __dso_hidden void ___start(void (*)(void), const Obj_Entry *,
			 struct ps_strings *);

#if !defined(__minix)
#define	write(fd, s, n)	__syscall(SYS_write, (fd), (s), (n))
#else
#define	write(fd, s, n) /* NO write() from here on minix */
#endif /* !defined(__minix) */

#define	_FATAL(str)				\
do {						\
	write(2, str, sizeof(str)-1);		\
	_exit(1);				\
} while (0)

#ifdef HAVE_INITFINI_ARRAY
/*
 * If we are using INIT_ARRAY/FINI_ARRAY and we are linked statically,
 * we have to process these instead of relying on RTLD to do it for us.
 *
 * Since we don't need .init or .fini sections, just code them in C
 * to make life easier.
 */
__weakref_visible const fptr_t preinit_array_start[1]
    __weak_reference(__preinit_array_start);
__weakref_visible const fptr_t preinit_array_end[1]
    __weak_reference(__preinit_array_end);
__weakref_visible const fptr_t init_array_start[1]
    __weak_reference(__init_array_start);
__weakref_visible const fptr_t init_array_end[1]
    __weak_reference(__init_array_end);
__weakref_visible const fptr_t fini_array_start[1]
    __weak_reference(__fini_array_start);
__weakref_visible const fptr_t fini_array_end[1]
    __weak_reference(__fini_array_end);

static inline void
_preinit(void)
{
	for (const fptr_t *f = preinit_array_start; f < preinit_array_end; f++) {
		(*f)();
	}
}

static inline void
_init(void)
{
	for (const fptr_t *f = init_array_start; f < init_array_end; f++) {
		(*f)();
	}
}

static void
_fini(void)
{
	for (const fptr_t *f = fini_array_start; f < fini_array_end; f++) {
		(*f)();
	}
}
#endif /* HAVE_INITFINI_ARRAY */

void
___start(void (*cleanup)(void),			/* from shared loader */
    const Obj_Entry *obj,			/* from shared loader */
    struct ps_strings *ps_strings)
{

	if (ps_strings == NULL)
		_FATAL("ps_strings missing\n");
	__ps_strings = ps_strings;

	environ = ps_strings->ps_envstr;

	if (ps_strings->ps_argvstr[0] != NULL) {
		char *c;
		__progname = ps_strings->ps_argvstr[0];
		for (c = ps_strings->ps_argvstr[0]; *c; ++c) {
			if (*c == '/')
				__progname = c + 1;
		}
	} else {
		__progname = empty_string;
	}

	if (&rtld_DYNAMIC != NULL) {
		if (obj == NULL)
			_FATAL("NULL Obj_Entry pointer in GOT\n");
		if (obj->magic != RTLD_MAGIC)
			_FATAL("Corrupt Obj_Entry pointer in GOT\n");
		if (obj->version != RTLD_VERSION)
			_FATAL("Dynamic linker version mismatch\n");
		atexit(cleanup);
	}

	_libc_init();

#ifdef HAVE_INITFINI_ARRAY
	_preinit();
#endif

#ifdef MCRT0
	atexit(_mcleanup);
	monstartup((u_long)&__eprol, (u_long)&__etext);
#endif

	atexit(_fini);
	_init();

	exit(main(ps_strings->ps_nargvstr, ps_strings->ps_argvstr, environ));
}
