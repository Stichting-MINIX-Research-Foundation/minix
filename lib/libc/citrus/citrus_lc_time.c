/* $NetBSD: citrus_lc_time.c,v 1.6 2012/03/04 21:14:55 tnozaki Exp $ */

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
__RCSID("$NetBSD: citrus_lc_time.c,v 1.6 2012/03/04 21:14:55 tnozaki Exp $");
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

/*
 * macro required by all template headers
 */
#define _PREFIX(name)		__CONCAT(_citrus_LC_TIME_, name)

#include "nb_lc_time_misc.h"
#include "citrus_lc_template_decl.h"

static __inline void
_citrus_LC_TIME_uninit(_TimeLocale *data)
{
	size_t i, j;

	_DIAGASSERT(data != NULL);

	for (i = ABDAY_IDX(ABDAY_1), j = ABDAY_IDX(ABDAY_7);  i <= j; ++i)
		free(__UNCONST(data->abday[i]));
	for (i = DAY_IDX(DAY_1),     j = DAY_IDX(DAY_7);      i <= j; ++i)
		free(__UNCONST(data->day[i]));
	for (i = ABMON_IDX(ABMON_1), j = ABMON_IDX(ABMON_12); i <= j; ++i)
		free(__UNCONST(data->abmon[i]));
	for (i = MON_IDX(MON_1),     j = MON_IDX(MON_12);     i <= j; ++i)
		free(__UNCONST(data->mon[i]));
	for (i = AM_PM_IDX(AM_STR),  j = AM_PM_IDX(PM_STR);   i <= j; ++i)
		free(__UNCONST(data->am_pm[i]));
	free(__UNCONST(data->d_t_fmt));
	free(__UNCONST(data->d_fmt));
	free(__UNCONST(data->t_fmt));
	free(__UNCONST(data->t_fmt_ampm));
}

#include "citrus_lc_time.h"

struct _citrus_LC_TIME_key {
	const char *name;
	size_t offset;
};

#define OFFSET(field) offsetof(_TimeLocale, field)
static const struct _citrus_LC_TIME_key keys[] = {
  { _CITRUS_LC_TIME_SYM_ABDAY_1,  OFFSET(abday[ABDAY_IDX(ABDAY_1)] ) },
  { _CITRUS_LC_TIME_SYM_ABDAY_2,  OFFSET(abday[ABDAY_IDX(ABDAY_2)] ) },
  { _CITRUS_LC_TIME_SYM_ABDAY_3,  OFFSET(abday[ABDAY_IDX(ABDAY_3)] ) },
  { _CITRUS_LC_TIME_SYM_ABDAY_4,  OFFSET(abday[ABDAY_IDX(ABDAY_4)] ) },
  { _CITRUS_LC_TIME_SYM_ABDAY_5,  OFFSET(abday[ABDAY_IDX(ABDAY_5)] ) },
  { _CITRUS_LC_TIME_SYM_ABDAY_6,  OFFSET(abday[ABDAY_IDX(ABDAY_6)] ) },
  { _CITRUS_LC_TIME_SYM_ABDAY_7,  OFFSET(abday[ABDAY_IDX(ABDAY_7)] ) },
  { _CITRUS_LC_TIME_SYM_DAY_1,    OFFSET(day[DAY_IDX(DAY_1)]       ) },
  { _CITRUS_LC_TIME_SYM_DAY_2,    OFFSET(day[DAY_IDX(DAY_2)]       ) },
  { _CITRUS_LC_TIME_SYM_DAY_3,    OFFSET(day[DAY_IDX(DAY_3)]       ) },
  { _CITRUS_LC_TIME_SYM_DAY_4,    OFFSET(day[DAY_IDX(DAY_4)]       ) },
  { _CITRUS_LC_TIME_SYM_DAY_5,    OFFSET(day[DAY_IDX(DAY_5)]       ) },
  { _CITRUS_LC_TIME_SYM_DAY_6,    OFFSET(day[DAY_IDX(DAY_6)]       ) },
  { _CITRUS_LC_TIME_SYM_DAY_7,    OFFSET(day[DAY_IDX(DAY_7)]       ) },
  { _CITRUS_LC_TIME_SYM_ABMON_1,  OFFSET(abmon[ABMON_IDX(ABMON_1)] ) },
  { _CITRUS_LC_TIME_SYM_ABMON_2,  OFFSET(abmon[ABMON_IDX(ABMON_2)] ) },
  { _CITRUS_LC_TIME_SYM_ABMON_3,  OFFSET(abmon[ABMON_IDX(ABMON_3)] ) },
  { _CITRUS_LC_TIME_SYM_ABMON_4,  OFFSET(abmon[ABMON_IDX(ABMON_4)] ) },
  { _CITRUS_LC_TIME_SYM_ABMON_5,  OFFSET(abmon[ABMON_IDX(ABMON_5)] ) },
  { _CITRUS_LC_TIME_SYM_ABMON_6,  OFFSET(abmon[ABMON_IDX(ABMON_6)] ) },
  { _CITRUS_LC_TIME_SYM_ABMON_7,  OFFSET(abmon[ABMON_IDX(ABMON_7)] ) },
  { _CITRUS_LC_TIME_SYM_ABMON_8,  OFFSET(abmon[ABMON_IDX(ABMON_8)] ) },
  { _CITRUS_LC_TIME_SYM_ABMON_9,  OFFSET(abmon[ABMON_IDX(ABMON_9)] ) },
  { _CITRUS_LC_TIME_SYM_ABMON_10, OFFSET(abmon[ABMON_IDX(ABMON_10)]) },
  { _CITRUS_LC_TIME_SYM_ABMON_11, OFFSET(abmon[ABMON_IDX(ABMON_11)]) },
  { _CITRUS_LC_TIME_SYM_ABMON_12, OFFSET(abmon[ABMON_IDX(ABMON_12)]) },
  { _CITRUS_LC_TIME_SYM_MON_1,    OFFSET(mon[MON_IDX(MON_1)]       ) },
  { _CITRUS_LC_TIME_SYM_MON_2,    OFFSET(mon[MON_IDX(MON_2)]       ) },
  { _CITRUS_LC_TIME_SYM_MON_3,    OFFSET(mon[MON_IDX(MON_3)]       ) },
  { _CITRUS_LC_TIME_SYM_MON_4,    OFFSET(mon[MON_IDX(MON_4)]       ) },
  { _CITRUS_LC_TIME_SYM_MON_5,    OFFSET(mon[MON_IDX(MON_5)]       ) },
  { _CITRUS_LC_TIME_SYM_MON_6,    OFFSET(mon[MON_IDX(MON_6)]       ) },
  { _CITRUS_LC_TIME_SYM_MON_7,    OFFSET(mon[MON_IDX(MON_7)]       ) },
  { _CITRUS_LC_TIME_SYM_MON_8,    OFFSET(mon[MON_IDX(MON_8)]       ) },
  { _CITRUS_LC_TIME_SYM_MON_9,    OFFSET(mon[MON_IDX(MON_9)]       ) },
  { _CITRUS_LC_TIME_SYM_MON_10,   OFFSET(mon[MON_IDX(MON_10)]      ) },
  { _CITRUS_LC_TIME_SYM_MON_11,   OFFSET(mon[MON_IDX(MON_11)]      ) },
  { _CITRUS_LC_TIME_SYM_MON_12,   OFFSET(mon[MON_IDX(MON_12)]      ) },
  { _CITRUS_LC_TIME_SYM_AM_STR,   OFFSET(am_pm[AM_PM_IDX(AM_STR)]  ) },
  { _CITRUS_LC_TIME_SYM_PM_STR,   OFFSET(am_pm[AM_PM_IDX(PM_STR)]  ) },
  { _CITRUS_LC_TIME_SYM_D_T_FMT,  OFFSET(d_t_fmt                   ) },
  { _CITRUS_LC_TIME_SYM_D_FMT,    OFFSET(d_fmt                     ) },
  { _CITRUS_LC_TIME_SYM_T_FMT,    OFFSET(t_fmt                     ) },
  { _CITRUS_LC_TIME_SYM_T_FMT_AMPM, OFFSET(t_fmt_ampm                ) },
  { NULL, 0 }
};

static __inline int
_citrus_LC_TIME_init_normal(_TimeLocale * __restrict data,
    struct _citrus_db * __restrict db)
{
        const struct _citrus_LC_TIME_key *key;
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
	return 0;

fatal:
	_citrus_LC_TIME_uninit(data);
	return EFTYPE;
}

static __inline int
_citrus_LC_TIME_init_fallback(_TimeLocale * __restrict data,
    struct _memstream * __restrict ms)
{
        const struct _citrus_LC_TIME_key *key;
	char **p;
	const char *s;
	size_t n;

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
	return 0;

fatal:
	_citrus_LC_TIME_uninit(data);
	return EFTYPE;
}

/*
 * macro required by citrus_lc_template.h
 */
#define _CATEGORY_DB		"LC_TIME"
#define _CATEGORY_MAGIC		_CITRUS_LC_TIME_MAGIC_1

#include "citrus_lc_template.h"
