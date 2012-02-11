/* $NetBSD: getprogname.c,v 1.3 2003/07/26 19:24:42 salo Exp $ */

/*
 * Copyright (c) 2001 Christopher G. Demetriou
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
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: getprogname.c,v 1.3 2003/07/26 19:24:42 salo Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#include <stdlib.h>
#include <string.h>

#ifdef __weak_alias
__weak_alias(getprogname, _getprogname)
__weak_alias(setprogname, _setprogname)
#endif

#if defined(__ELF__)
extern const char *__progname;

const char *
getprogname(void)
{

        return (__progname);
}

void
setprogname(const char *progname)
{
        const char *p;

        p = strrchr(progname, '/');
        if (p != NULL)
                __progname = p + 1;
        else
                __progname = progname;
}
#else
static const char *theprogname = NULL;
extern const char **__prognamep;	/* Copy of argv[]. */
extern int __argc;			/* Copy of argc. */

const char *
getprogname(void)
{
	const char *pn = NULL, *component;
	if(theprogname)
		return theprogname;
	if(__argc > 0 && __prognamep)
		pn = __prognamep[0];
	else
		return NULL;

	if((component=strrchr(pn, '/')))
		return component+1;
	return pn;
}

void
setprogname(const char *newprogname)
{
	theprogname = newprogname;
}
#endif
