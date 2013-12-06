/* $NetBSD: newlocale.c,v 1.3 2013/09/13 13:13:32 joerg Exp $ */

/*-
 * Copyright (c)2008, 2011 Citrus Project,
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
__RCSID("$NetBSD: newlocale.c,v 1.3 2013/09/13 13:13:32 joerg Exp $");

#include "namespace.h"
#include <assert.h>
#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include "setlocale_local.h"

__weak_alias(newlocale, _newlocale)

locale_t
newlocale(int mask, const char *name, locale_t src)
{
	struct _locale *dst;
	char head[_LOCALENAME_LEN_MAX * (_LC_LAST - 1)], *tail;
	const char *tokens[_LC_LAST - 1];
	_locale_set_t l;
	int i, howmany, categories[_LC_LAST - 1];

	if (name == NULL)
		name = _C_LOCALE;
	dst = malloc(sizeof(*dst));
	if (dst == NULL)
		return (locale_t)NULL;
	if (src == NULL)
		src = _current_locale();
	memcpy(dst, src, sizeof(*src));
	strlcpy(&head[0], name, sizeof(head));
	tokens[0] = (const char *)&head[0];
	tail = strchr(tokens[0], '/');
	if (tail == NULL) {
		for (i = 1; i < _LC_LAST; ++i) {
			if (mask & (1 << i)) {
				l = _find_category(i);
				_DIAGASSERT(l != NULL);
				(*l)(tokens[0], dst);
			}
		}
	} else {
		*tail++ = '\0';
		howmany = 0;
		for (i = 1; i < _LC_LAST; ++i) {
			if (mask & (1 << i))
				categories[howmany++] = i;
		}
		if (howmany-- > 0) {
			for (i = 1; i < howmany; ++i) {
				tokens[i] = (const char *)tail;
				tail = strchr(tokens[i], '/');
				if (tail == NULL) {
					free(dst);
					return NULL;
				}
			}
			tokens[howmany] = tail;
			tail = strchr(tokens[howmany], '/');
			if (tail != NULL) {
				free(dst);
				return NULL;
			}
			for (i = 0; i <= howmany; ++i) {
				l = _find_category(categories[i]);
				_DIAGASSERT(l != NULL);
				(*l)(tokens[i], dst);
			}
		}
	}
	if (_setlocale_cache(dst, NULL)) {
		free(dst);
		return NULL;
	}
	return (locale_t)dst;
}
