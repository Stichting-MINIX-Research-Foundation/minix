/* $NetBSD: citrus_lc_numeric.c,v 1.5 2012/03/04 21:14:55 tnozaki Exp $ */

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
__RCSID("$NetBSD: citrus_lc_numeric.c,v 1.5 2012/03/04 21:14:55 tnozaki Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include "reentrant.h"
#include <sys/types.h>
#include <sys/localedef.h>
#include <sys/queue.h>
#include <assert.h>
#include <errno.h>
#include <langinfo.h>
#include <limits.h>
#define __SETLOCALE_SOURCE__
#include <locale.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "setlocale_local.h"

#include "citrus_namespace.h"
#include "citrus_types.h"
#include "citrus_bcs.h"
#include "citrus_region.h"
#include "citrus_lookup.h"
#include "citrus_aliasname_local.h"
#include "citrus_module.h"
#include "citrus_mmap.h"
#include "citrus_hash.h"
#include "citrus_db.h"
#include "citrus_db_hash.h"
#include "citrus_memstream.h"
#include "runetype_local.h"

#include "fix_grouping.h"
#include "citrus_fix_grouping.h"

/*
 * macro required by all template headers
 */
#define _PREFIX(name)		__CONCAT(_citrus_LC_NUMERIC_, name)

#include "nb_lc_numeric_misc.h"
#include "citrus_lc_template_decl.h"

static __inline void
_citrus_LC_NUMERIC_uninit(_NumericLocale *data)
{
	free(__UNCONST(data->decimal_point));
	free(__UNCONST(data->thousands_sep));
	free(__UNCONST(data->grouping));
}

#include "citrus_lc_numeric.h"

struct _citrus_LC_NUMERIC_key {
	const char *name;
	size_t offset;
};

#define OFFSET(field) (offsetof(_NumericLocale, field))
static const struct _citrus_LC_NUMERIC_key keys[] = {
  { _CITRUS_LC_NUMERIC_SYM_DECIMAL_POINT, OFFSET(decimal_point) },
  { _CITRUS_LC_NUMERIC_SYM_THOUSANDS_SEP, OFFSET(thousands_sep) },
  { _CITRUS_LC_NUMERIC_SYM_GROUPING,      OFFSET(grouping     ) },
  { NULL, 0 }
};

static __inline int
_citrus_LC_NUMERIC_init_normal(_NumericLocale * __restrict data,
    struct _citrus_db * __restrict db)
{
	const struct _citrus_LC_NUMERIC_key *key;
	char **p;
	const char *s;

	_DIAGASSERT(data != NULL);
	_DIAGASSERT(db != NULL);

	memset(data, 0, sizeof(*data));
	for (key = &keys[0]; key->name != NULL; ++key) {
		if (_db_lookupstr_by_s(db, key->name, &s, NULL))
			goto fatal;
		p = (char **)(void *)
		    (((char *)(void *)data) + key->offset);
		*p = strdup(s);
		if (*p == NULL)
			goto fatal;
	}
	_CITRUS_FIXUP_CHAR_MAX_MD(data->grouping);

	return 0;

fatal:
	_citrus_LC_NUMERIC_uninit(data);
	return EFTYPE;
}

static __inline int
_citrus_LC_NUMERIC_init_fallback(_NumericLocale * __restrict data,
    struct _memstream * __restrict ms)
{
	const struct _citrus_LC_NUMERIC_key *key;
	char **p;
	const char *s;
	size_t n;

	_DIAGASSERT(data != NULL);
	_DIAGASSERT(ms != NULL);

	memset(data, 0, sizeof(*data));
	for (key = &keys[0]; key->name != NULL; ++key) {
		if ((s = _memstream_getln(ms, &n)) == NULL)
			goto fatal;
		p = (char **)(void *)
		    (((char *)(void *)data) + key->offset);
		*p = strndup(s, n - 1);
		if (*p == NULL)
			goto fatal;
	}
	data->grouping =
	    __fix_locale_grouping_str(data->grouping);

	return 0;

fatal:
	_citrus_LC_NUMERIC_uninit(data);
	return EFTYPE;
}

/*
 * macro required by citrus_lc_template.h
 */
#define _CATEGORY_DB		"LC_NUMERIC"
#define _CATEGORY_MAGIC		_CITRUS_LC_NUMERIC_MAGIC_1

#include "citrus_lc_template.h"
