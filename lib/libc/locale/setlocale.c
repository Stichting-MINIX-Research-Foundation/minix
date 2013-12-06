/* $NetBSD: setlocale.c,v 1.64 2013/09/13 13:13:32 joerg Exp $ */

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
__RCSID("$NetBSD: setlocale.c,v 1.64 2013/09/13 13:13:32 joerg Exp $");
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/localedef.h>
#include <locale.h>
#include <limits.h>
#include <paths.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "setlocale_local.h"

const char *_PathLocale = NULL;

static _locale_set_t all_categories[_LC_LAST] = {
	[LC_ALL     ] = &_generic_LC_ALL_setlocale,
	[LC_COLLATE ] = &_dummy_LC_COLLATE_setlocale,
	[LC_CTYPE   ] = &_citrus_LC_CTYPE_setlocale,
	[LC_MONETARY] = &_citrus_LC_MONETARY_setlocale,
	[LC_NUMERIC ] = &_citrus_LC_NUMERIC_setlocale,
	[LC_TIME    ] = &_citrus_LC_TIME_setlocale,
	[LC_MESSAGES] = &_citrus_LC_MESSAGES_setlocale,
};

/* XXX Consider locking the list. Race condition leaks memory. */
static SLIST_HEAD(, _locale_cache_t) caches = {
    __UNCONST(&_C_cache)
};

int
_setlocale_cache(locale_t loc, struct _locale_cache_t *cache)
{
	const char *monetary_name = loc->part_name[LC_MONETARY];
	const char *numeric_name = loc->part_name[LC_NUMERIC];
	_NumericLocale *numeric = loc->part_impl[LC_NUMERIC];
	_MonetaryLocale *monetary = loc->part_impl[LC_MONETARY];
	struct lconv *ldata;

	struct _locale_cache_t *old_cache;

	SLIST_FOREACH(old_cache, &caches, cache_link) {
		if (monetary_name != old_cache->monetary_name &&
		    strcmp(monetary_name, old_cache->monetary_name) != 0)
			continue;
		if (numeric_name != old_cache->numeric_name &&
		    strcmp(numeric_name, old_cache->numeric_name) != 0)
			continue;
		loc->cache = old_cache;
		free(cache);
		return 0;
	}

	if (cache == NULL) {
		cache = malloc(sizeof(*cache));
		if (cache == NULL)
			return -1;
	}

	cache->monetary_name = monetary_name;
	cache->numeric_name = numeric_name;
	ldata = &cache->ldata;

	ldata->decimal_point = __UNCONST(numeric->decimal_point);
	ldata->thousands_sep = __UNCONST(numeric->thousands_sep);
	ldata->grouping      = __UNCONST(numeric->grouping);

	ldata->int_curr_symbol   = __UNCONST(monetary->int_curr_symbol);
	ldata->currency_symbol   = __UNCONST(monetary->currency_symbol);
	ldata->mon_decimal_point = __UNCONST(monetary->mon_decimal_point);
	ldata->mon_thousands_sep = __UNCONST(monetary->mon_thousands_sep);
	ldata->mon_grouping      = __UNCONST(monetary->mon_grouping);
	ldata->positive_sign     = __UNCONST(monetary->positive_sign);
	ldata->negative_sign     = __UNCONST(monetary->negative_sign);

	ldata->int_frac_digits    = monetary->int_frac_digits;
	ldata->frac_digits        = monetary->frac_digits;
	ldata->p_cs_precedes      = monetary->p_cs_precedes;
	ldata->p_sep_by_space     = monetary->p_sep_by_space;
	ldata->n_cs_precedes      = monetary->n_cs_precedes;
	ldata->n_sep_by_space     = monetary->n_sep_by_space;
	ldata->p_sign_posn        = monetary->p_sign_posn;
	ldata->n_sign_posn        = monetary->n_sign_posn;
	ldata->int_p_cs_precedes  = monetary->int_p_cs_precedes;
	ldata->int_n_cs_precedes  = monetary->int_n_cs_precedes;
	ldata->int_p_sep_by_space = monetary-> int_p_sep_by_space;
	ldata->int_n_sep_by_space = monetary->int_n_sep_by_space;
	ldata->int_p_sign_posn    = monetary->int_p_sign_posn;
	ldata->int_n_sign_posn    = monetary->int_n_sign_posn;
	SLIST_INSERT_HEAD(&caches, cache, cache_link);

	loc->cache = cache;
	return 0;
}

_locale_set_t
_find_category(int category)
{
	static int initialised;

	if (!initialised) {
		if (issetugid() || ((_PathLocale == NULL &&
		    (_PathLocale = getenv("PATH_LOCALE")) == NULL) ||
		    *_PathLocale == '\0'))
			_PathLocale = _PATH_LOCALE;
		initialised = 1;
	}

	if (category >= LC_ALL && category < _LC_LAST)
		return all_categories[category];
	return NULL;
}

const char *
_get_locale_env(const char *category)
{
	const char *name;

	/* 1. check LC_ALL */
	name = (const char *)getenv("LC_ALL");
	if (name == NULL || *name == '\0') {
		/* 2. check LC_* */
		name = (const char *)getenv(category);
		if (name == NULL || *name == '\0') {
			/* 3. check LANG */
			name = getenv("LANG");
		}
	}
	if (name == NULL || *name == '\0' || strchr(name, '/'))
		/* 4. if none is set, fall to "C" */
		name = _C_LOCALE;
	return name;
}

char *
__setlocale(int category, const char *name)
{
	_locale_set_t sl;
	locale_t loc;
	struct _locale_cache_t *cache;
	const char *result;

	sl = _find_category(category);
	if (sl == NULL)
		return NULL;
	cache = malloc(sizeof(*cache));
	if (cache == NULL)
		return NULL;
	loc = _current_locale();
	result = (*sl)(name, loc);
	_setlocale_cache(loc, cache);
	return __UNCONST(result);
}

char *
setlocale(int category, const char *locale)
{

	/* locale may be NULL */

	__mb_len_max_runtime = MB_LEN_MAX;
	return __setlocale(category, locale);
}
