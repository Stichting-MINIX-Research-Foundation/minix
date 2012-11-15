/* $NetBSD: citrus_lc_monetary.c,v 1.5 2012/03/04 21:14:55 tnozaki Exp $ */

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
__RCSID("$NetBSD: citrus_lc_monetary.c,v 1.5 2012/03/04 21:14:55 tnozaki Exp $");
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
#define _PREFIX(name)		__CONCAT(_citrus_LC_MONETARY_, name)

#include "nb_lc_monetary_misc.h"
#include "citrus_lc_template_decl.h"

static __inline void
_citrus_LC_MONETARY_uninit(_MonetaryLocale *data)
{
	_DIAGASSERT(data != NULL);

	free(__UNCONST(data->int_curr_symbol));
	free(__UNCONST(data->currency_symbol));
	free(__UNCONST(data->mon_decimal_point));
	free(__UNCONST(data->mon_thousands_sep));
	free(__UNCONST(data->mon_grouping));
	free(__UNCONST(data->positive_sign));
	free(__UNCONST(data->negative_sign));
}

#include "citrus_lc_monetary.h"

struct _citrus_LC_MONETARY_key {
	const char *name;
	size_t offset;
};

#define OFFSET(field) (offsetof(_MonetaryLocale, field))
static const struct _citrus_LC_MONETARY_key keys_string[] = {
  { _CITRUS_LC_MONETARY_SYM_INT_CURR_SYMBOL,    OFFSET(int_curr_symbol   ) },
  { _CITRUS_LC_MONETARY_SYM_CURRENCY_SYMBOL,    OFFSET(currency_symbol   ) },
  { _CITRUS_LC_MONETARY_SYM_MON_DECIMAL_POINT,  OFFSET(mon_decimal_point ) },
  { _CITRUS_LC_MONETARY_SYM_MON_THOUSANDS_SEP,  OFFSET(mon_thousands_sep ) },
  { _CITRUS_LC_MONETARY_SYM_MON_GROUPING,       OFFSET(mon_grouping      ) },
  { _CITRUS_LC_MONETARY_SYM_POSITIVE_SIGN,      OFFSET(positive_sign     ) },
  { _CITRUS_LC_MONETARY_SYM_NEGATIVE_SIGN,      OFFSET(negative_sign     ) },
  { NULL, (size_t)0 }
};
static const struct _citrus_LC_MONETARY_key keys_char[] = {
  { _CITRUS_LC_MONETARY_SYM_INT_FRAC_DIGITS,    OFFSET(int_frac_digits   ) },
  { _CITRUS_LC_MONETARY_SYM_FRAC_DIGITS,        OFFSET(frac_digits       ) },
  { _CITRUS_LC_MONETARY_SYM_P_CS_PRECEDES,      OFFSET(p_cs_precedes     ) },
  { _CITRUS_LC_MONETARY_SYM_P_SEP_BY_SPACE,     OFFSET(p_sep_by_space    ) },
  { _CITRUS_LC_MONETARY_SYM_N_CS_PRECEDES,      OFFSET(n_cs_precedes     ) },
  { _CITRUS_LC_MONETARY_SYM_N_SEP_BY_SPACE,     OFFSET(n_sep_by_space    ) },
  { _CITRUS_LC_MONETARY_SYM_P_SIGN_POSN,        OFFSET(p_sign_posn       ) },
  { _CITRUS_LC_MONETARY_SYM_N_SIGN_POSN,        OFFSET(n_sign_posn       ) },
  { _CITRUS_LC_MONETARY_SYM_INT_P_CS_PRECEDES,  OFFSET(int_p_cs_precedes ) },
  { _CITRUS_LC_MONETARY_SYM_INT_N_CS_PRECEDES,  OFFSET(int_n_cs_precedes ) },
  { _CITRUS_LC_MONETARY_SYM_INT_P_SEP_BY_SPACE, OFFSET(int_p_sep_by_space) },
  { _CITRUS_LC_MONETARY_SYM_INT_N_SEP_BY_SPACE, OFFSET(int_n_sep_by_space) },
  { _CITRUS_LC_MONETARY_SYM_INT_P_SIGN_POSN,    OFFSET(int_p_sign_posn   ) },
  { _CITRUS_LC_MONETARY_SYM_INT_N_SIGN_POSN,    OFFSET(int_n_sign_posn   ) },
  { NULL, (size_t)0 }
};

static __inline int
_citrus_LC_MONETARY_init_normal(_MonetaryLocale * __restrict data,
    struct _citrus_db * __restrict db)
{
	const struct _citrus_LC_MONETARY_key *key;
	char **p_string, *p_char;
	const char *s;
	uint8_t u8;

	_DIAGASSERT(data != NULL);
	_DIAGASSERT(db != NULL);

	memset(data, 0, sizeof(*data));
	for (key = &keys_string[0]; key->name != NULL; ++key) {
		if (_db_lookupstr_by_s(db, key->name, &s, NULL))
			goto fatal;
		p_string = (char **)(void *)
		    (((char *)(void *)data) + key->offset);
		*p_string = strdup(s);
		if (*p_string == NULL)
			goto fatal;
	}
	for (key = &keys_char[0]; key->name != NULL; ++key) {
		if (_db_lookup8_by_s(db, key->name, &u8, NULL))
			goto fatal;
		p_char = ((char *)(void *)data) + key->offset;
		*p_char = (char)(unsigned char)u8;
	}
	_CITRUS_FIXUP_CHAR_MAX_MD(data->mon_grouping);

	return 0;

fatal:
	_citrus_LC_MONETARY_uninit(data);
	return EFTYPE;
}

static __inline int
_citrus_LC_MONETARY_init_fallback(_MonetaryLocale * __restrict data,
    struct _memstream * __restrict ms)
{
	const struct _citrus_LC_MONETARY_key *key;
	char **p_string, *p_char;
	const char *s;
	size_t n;
	char *t;
	long int l;

	_DIAGASSERT(data != NULL);
	_DIAGASSERT(ms != NULL);

	memset(data, 0, sizeof(*data));
	for (key = &keys_string[0]; key->name != NULL; ++key) {
		if ((s = _memstream_getln(ms, &n)) == NULL)
			goto fatal;
		p_string = (char **)(void *)
		    (((char *)(void *)data) + key->offset);
		*p_string = strndup(s, n - 1);
		if (*p_string == NULL)
			goto fatal;
	}
	for (key = &keys_char[0]; key->name != NULL; ++key) {
		if ((s = _memstream_getln(ms, &n)) == NULL)
			goto fatal;
		t = strndup(s, n - 1);
		if (t == NULL)
			goto fatal;
		s = (const char *)t;
		l = _bcs_strtol(s, &t, 0);
		if (s == t || l < 0 || l > 0x7fL) {
			free(t);
			goto fatal;
		}
		free(t);
		p_char = ((char *)(void *)data) + key->offset;
		*p_char = (char)(l & 0x7fL);
	}
        data->mon_grouping =
	    __fix_locale_grouping_str(data->mon_grouping);
	return 0;

fatal:
	_citrus_LC_MONETARY_uninit(data);
	return EFTYPE;
}

/*
 * macro required by citrus_lc_template.h
 */
#define _CATEGORY_DB		"LC_MONETARY"
#define _CATEGORY_MAGIC		_CITRUS_LC_MONETARY_MAGIC_1

#include "citrus_lc_template.h"
