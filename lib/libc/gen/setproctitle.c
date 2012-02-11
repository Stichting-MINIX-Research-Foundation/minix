/* $NetBSD: setproctitle.c,v 1.22 2008/01/03 04:26:27 christos Exp $ */

/*
 * Copyright (c) 1994, 1995 Christopher G. Demetriou
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
__RCSID("$NetBSD: setproctitle.c,v 1.22 2008/01/03 04:26:27 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/param.h>
#include <sys/types.h>
#include <sys/exec.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __weak_alias
__weak_alias(setproctitle,_setproctitle)
#endif

#define	MAX_PROCTITLE	2048

/*
 * For compatibility with old versions of crt0 that didn't define __ps_strings,
 * define it as a common here.
 */
struct ps_strings *__ps_strings;

void
setproctitle(const char *fmt, ...)
{
	static char buf[MAX_PROCTITLE], *bufp;
	const char *pname = getprogname();

	if (fmt != NULL) {
		int len = snprintf(buf, sizeof(buf), "%s: ", pname);
		if (len >= 0) {
			va_list ap;

			va_start(ap, fmt);
			(void)vsnprintf(buf + len, sizeof(buf) - len, fmt, ap);
			va_end(ap);
		}
	} else
		(void)snprintf(buf, sizeof(buf), "%s", pname);

	bufp = buf;

#ifdef	USRSTACK
	/*
	 * For compatibility with old versions of crt0 and old kernels, set
	 * __ps_strings to a default value if it's null.
	 * But only if USRSTACK is defined.  It might not be defined if
	 * user-level code can not assume it's a constant (i.e. m68k).
	 */
	if (__ps_strings == 0)
		__ps_strings = PS_STRINGS;
#endif	/* USRSTACK */

	if (__ps_strings != 0) {
		__ps_strings->ps_nargvstr = 1;
		__ps_strings->ps_argvstr = &bufp;
	}
}
