/*	$NetBSD: getprotobyname_r.c,v 1.3 2005/04/18 19:39:45 kleink Exp $	*/

/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)getprotoname.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: getprotobyname_r.c,v 1.3 2005/04/18 19:39:45 kleink Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#include <assert.h>
#include <netdb.h>
#include <string.h>

#include "protoent.h"

#ifdef __weak_alias
__weak_alias(getprotobyname_r,_getprotobyname_r)
#endif

struct protoent *
getprotobyname_r(const char *name, struct protoent *pr,
    struct protoent_data *pd)
{
	struct protoent *p;
	char **cp;

	_DIAGASSERT(name != NULL);

	setprotoent_r(pd->stayopen, pd);
	while ((p = getprotoent_r(pr, pd)) != NULL) {
		if (strcmp(p->p_name, name) == 0)
			break;
		for (cp = p->p_aliases; *cp != NULL; cp++)
			if (strcmp(*cp, name) == 0)
				goto found;
	}
found:
	if (!pd->stayopen)
		if (pd->fp != NULL) {
			(void)fclose(pd->fp);
			pd->fp = NULL;
		}
	return p;
}
