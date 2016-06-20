/*	$NetBSD: textdomain.c,v 1.14 2015/05/29 12:26:28 christos Exp $	*/

/*-
 * Copyright (c) 2000, 2001 Citrus Project,
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: textdomain.c,v 1.14 2015/05/29 12:26:28 christos Exp $");

#include <sys/param.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libintl.h>
#include "libintl_local.h"
#include "pathnames.h"

static struct domainbinding __default_binding = {
	.path = { _PATH_TEXTDOMAIN },
	.domainname = { DEFAULT_DOMAINNAME },
};
struct domainbinding *__bindings = &__default_binding;
char __current_domainname[PATH_MAX] = DEFAULT_DOMAINNAME;

static struct domainbinding *domainbinding_lookup(const char *, int);

/*
 * set the default domainname for dcngettext() and friends.
 */
char *
textdomain(const char *domainname)
{

	/* NULL pointer gives the current setting */
	if (!domainname)
		return __current_domainname;

	/* empty string sets the value back to the default */
	if (!*domainname) {
		strlcpy(__current_domainname, DEFAULT_DOMAINNAME,
		    sizeof(__current_domainname));
	} else {
		strlcpy(__current_domainname, domainname,
		    sizeof(__current_domainname));
	}
	return __current_domainname;
}

char *
bindtextdomain(const char *domainname, const char *dirname)
{
	struct domainbinding *p;

	/* NULL pointer or empty string returns NULL with no operation */
	if (!domainname || !*domainname)
		return NULL;

	if (dirname && (strlen(dirname) + 1 > sizeof(p->path)))
		return NULL;

#if 0
	/* disallow relative path */
	if (dirname[0] != '/')
		return NULL;
#endif

	if (strlen(domainname) + 1 > sizeof(p->domainname))
		return NULL;

	p = domainbinding_lookup(domainname, (dirname != NULL));

	if (!dirname) {
		if (p)
			return (p->path);
		else
			return (char *)__UNCONST(_PATH_TEXTDOMAIN);
	}

	strlcpy(p->path, dirname, sizeof(p->path));
	p->mohandle.mo.mo_magic = 0; /* invalidate current mapping */

	return (p->path);
}

char *
bind_textdomain_codeset(const char *domainname, const char *codeset)
{
	struct domainbinding *p;

	p = domainbinding_lookup(domainname, (codeset != NULL));
	if (p == NULL)
		return NULL;

	if (codeset) {
		free(p->codeset);
		p->codeset = strdup(codeset);
	}

	return p->codeset;
}

/*
 * lookup binding for the domainname
 */
static struct domainbinding *
domainbinding_lookup(const char *domainname, int alloc)
{
	struct domainbinding *p;

	for (p = __bindings; p; p = p->next)
		if (strcmp(p->domainname, domainname) == 0)
			break;

	if (!p && alloc) {
		p = (struct domainbinding *)malloc(sizeof(*p));
		if (!p)
			return NULL;
		memset(p, 0, sizeof(*p));
		p->next = __bindings;
		strlcpy(p->domainname, domainname, sizeof(p->domainname));
		__bindings = p;
	}

	return p;
}
