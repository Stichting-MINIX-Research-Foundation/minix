/* $NetBSD: generic_lc_all.c,v 1.5 2013/04/14 23:30:16 joerg Exp $ */

/*-
 * Copyright (c)2008 Citrus Project,
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
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: generic_lc_all.c,v 1.5 2013/04/14 23:30:16 joerg Exp $");
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <assert.h>
#include <langinfo.h>
#define __SETLOCALE_SOURCE__
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "setlocale_local.h"

/*
 * macro required by all template headers
 */
#define _PREFIX(name)	__CONCAT(_generic_LC_ALL_, name)

#include "generic_lc_template_decl.h"

const char *
_generic_LC_ALL_setlocale(const char * __restrict name,
    struct _locale * __restrict locale)
{
	_locale_set_t sl;
	char head[_LOCALENAME_LEN_MAX * (_LC_LAST - 1)], *tail;
	const char *tokens[_LC_LAST], *s, *t;
	int load_locale_success, i, j;

	sl = _find_category(1);
	_DIAGASSERT(sl != NULL);
	load_locale_success = 0;
	if (name != NULL) {
		strlcpy(&head[0], name, sizeof(head));
		tokens[1] = &head[0];
		tail = strchr(tokens[1], '/');
		if (tail == NULL) {
			for (i = 2; i < _LC_LAST; ++i)
				tokens[i] = tokens[1];
		} else {
			*tail++ = '\0';
			for (i = 2; i < _LC_LAST - 1; ++i) {
				tokens[i] = (const char *)tail;
				tail = strchr(tokens[i], '/');
				if (tail == NULL)
					return NULL;
				*tail++ = '\0';
			}
			tokens[_LC_LAST - 1] = (const char *)tail;
			tail = strchr(tokens[i], '/');
			if (tail != NULL)
				return NULL;
		}
		if ((*sl)(tokens[1], locale) != NULL)
			load_locale_success = 1;
	}
	s = (*sl)(NULL, locale);
	_DIAGASSERT(s != NULL);
	strlcpy(&locale->query[0], s, sizeof(locale->query));
	for (i = 2, j = 0; i < _LC_LAST; ++i) {
		sl = _find_category(i);
		_DIAGASSERT(sl != NULL);
		if (name != NULL) {
			if ((*sl)(tokens[i], locale) != NULL)
				load_locale_success = 1;
		}
		t = (*sl)(NULL, locale);
		_DIAGASSERT(t != NULL);
		if (j == 0) {
			if (!strcmp(s, t))
				continue;
			for (j = 2; j < i; ++j) {
				strlcat(&locale->query[0], "/",
				    sizeof(locale->query));
				strlcat(&locale->query[0], s,
				    sizeof(locale->query));
			}
		}
		strlcat(&locale->query[0], "/", sizeof(locale->query));
		strlcat(&locale->query[0], t, sizeof(locale->query));
	}
	if (name != NULL && !load_locale_success)
		return NULL;
	return (const char *)&locale->query[0];
}

