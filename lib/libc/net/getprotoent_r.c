/*	$NetBSD: getprotoent_r.c,v 1.6 2011/10/15 23:00:02 christos Exp $	*/

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
static char sccsid[] = "@(#)getprotoent.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: getprotoent_r.c,v 1.6 2011/10/15 23:00:02 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "protoent.h"

#ifdef __weak_alias
__weak_alias(endprotoent_r,_endprotoent_r)
__weak_alias(getprotoent_r,_getprotoent_r)
__weak_alias(setprotoent_r,_setprotoent_r)
#endif

void
setprotoent_r(int f, struct protoent_data *pd)
{
	if (pd->fp == NULL)
		pd->fp = fopen(_PATH_PROTOCOLS, "re");
	else
		rewind(pd->fp);
	pd->stayopen |= f;
}

void
endprotoent_r(struct protoent_data *pd)
{
	if (pd->fp) {
		(void)fclose(pd->fp);
		pd->fp = NULL;
	}
	if (pd->aliases) {
		free(pd->aliases);
		pd->aliases = NULL;
		pd->maxaliases = 0;
	}
	if (pd->line) {
		free(pd->line);
		pd->line = NULL;
	}
	pd->stayopen = 0;
}

struct protoent *
getprotoent_r(struct protoent *pr, struct protoent_data *pd)
{
	char *p, *cp, **q;
	size_t i = 0;
	int oerrno;

	if (pd->fp == NULL && (pd->fp = fopen(_PATH_PROTOCOLS, "re")) == NULL)
		return NULL;

	for (;;) {
		if (pd->line)
			free(pd->line);
		pd->line = fparseln(pd->fp, NULL, NULL, NULL,
		    FPARSELN_UNESCALL);
		if (pd->line == NULL)
			return NULL;
		pr->p_name = p = pd->line;
		cp = strpbrk(p, " \t");
		if (cp == NULL)
			continue;
		*cp++ = '\0';
		while (*cp == ' ' || *cp == '\t')
			cp++;
		p = strpbrk(cp, " \t");
		if (p != NULL)
			*p++ = '\0';
		pr->p_proto = atoi(cp);
		if (pd->aliases == NULL) {
			pd->maxaliases = 10;
			pd->aliases = malloc(pd->maxaliases * sizeof(char *));
			if (pd->aliases == NULL) {
				oerrno = errno;
				endprotoent_r(pd);
				errno = oerrno;
				return NULL;
			}
		}
		q = pr->p_aliases = pd->aliases;
		if (p != NULL) {
			cp = p;
			while (cp && *cp) {
				if (*cp == ' ' || *cp == '\t') {
					cp++;
					continue;
				}
				if (i == pd->maxaliases - 2) {
					pd->maxaliases *= 2;
					q = realloc(q,
					    pd->maxaliases * sizeof(char *));
					if (q == NULL) {
						oerrno = errno;
						endprotoent_r(pd);
						errno = oerrno;
						return NULL;
					}
					pr->p_aliases = pd->aliases = q;
				}
				q[i++] = cp;

				cp = strpbrk(cp, " \t");
				if (cp != NULL)
					*cp++ = '\0';
			}
		}
		q[i] = NULL;
		return pr;
	}
}
