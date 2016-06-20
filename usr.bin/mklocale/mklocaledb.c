/* $NetBSD: mklocaledb.c,v 1.4 2015/06/16 22:54:10 christos Exp $ */

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

/*
 * XXX TEMPORARY IMPLEMENTATION.
 * don't waste your time, all we need is localedef(1).
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#if !defined(lint)
__RCSID("$NetBSD: mklocaledb.c,v 1.4 2015/06/16 22:54:10 christos Exp $");
#endif /* not lint */

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fix_grouping.h"

#include "citrus_namespace.h"
#include "citrus_bcs.h"
#include "citrus_types.h"
#include "citrus_region.h"
#include "citrus_db_factory.h"
#include "citrus_db_hash.h"
#include "citrus_db.h"

#include "citrus_lc_monetary.h"
#include "citrus_lc_numeric.h"
#include "citrus_lc_time.h"
#include "citrus_lc_messages.h"

void mklocaledb(const char *, FILE *, FILE *);

/*
 * TODO: -d debug options's output.
 */
extern int debug;
extern void usage(void);

static int
save_as_string(struct _db_factory *df,
    const char *key, const char *value)
{
	return _db_factory_addstr_by_s(df, key, value);
}

static int
save_as_grouping(struct _db_factory *df,
    const char *key, const char *value)
{
	value = __fix_locale_grouping_str(value);
	return _db_factory_addstr_by_s(df, key, value);
}

static int
save_as_uint8(struct _db_factory *df,
    const char *key, const char *head)
{
	char *tail;
	unsigned long int value;
	uint8_t u8;

	value = _bcs_strtoul(head, &tail, 0);
	if (head == tail || value > 0xFF)
		return 1;
	u8 = (uint8_t)(value & 0xFF);
	return _db_factory_add8_by_s(df, key, u8);
}

typedef struct {
	const char *key;
	int (*save)(struct _db_factory *,
	    const char *, const char *);
} token_t;

typedef struct {
	const char *magic, * vers_sym;
	uint32_t version;
	const token_t tokens[];
} category_t;

static const category_t lc_monetary = {
    _CITRUS_LC_MONETARY_MAGIC_1,
    _CITRUS_LC_MONETARY_SYM_VERSION,
    _CITRUS_LC_MONETARY_VERSION,
    {
	{ _CITRUS_LC_MONETARY_SYM_INT_CURR_SYMBOL,    &save_as_string   },
	{ _CITRUS_LC_MONETARY_SYM_CURRENCY_SYMBOL,    &save_as_string   },
	{ _CITRUS_LC_MONETARY_SYM_MON_DECIMAL_POINT,  &save_as_string   },
	{ _CITRUS_LC_MONETARY_SYM_MON_THOUSANDS_SEP,  &save_as_string   },
	{ _CITRUS_LC_MONETARY_SYM_MON_GROUPING,       &save_as_grouping },
	{ _CITRUS_LC_MONETARY_SYM_POSITIVE_SIGN,      &save_as_string   },
	{ _CITRUS_LC_MONETARY_SYM_NEGATIVE_SIGN,      &save_as_string   },
	{ _CITRUS_LC_MONETARY_SYM_INT_FRAC_DIGITS,    &save_as_uint8    },
	{ _CITRUS_LC_MONETARY_SYM_FRAC_DIGITS,        &save_as_uint8    },
	{ _CITRUS_LC_MONETARY_SYM_P_CS_PRECEDES,      &save_as_uint8    },
	{ _CITRUS_LC_MONETARY_SYM_P_SEP_BY_SPACE,     &save_as_uint8    },
	{ _CITRUS_LC_MONETARY_SYM_N_CS_PRECEDES,      &save_as_uint8    },
	{ _CITRUS_LC_MONETARY_SYM_N_SEP_BY_SPACE,     &save_as_uint8    },
	{ _CITRUS_LC_MONETARY_SYM_P_SIGN_POSN,        &save_as_uint8    },
	{ _CITRUS_LC_MONETARY_SYM_N_SIGN_POSN,        &save_as_uint8    },
	{ _CITRUS_LC_MONETARY_SYM_INT_P_CS_PRECEDES,  &save_as_uint8    },
	{ _CITRUS_LC_MONETARY_SYM_INT_P_SEP_BY_SPACE, &save_as_uint8    },
	{ _CITRUS_LC_MONETARY_SYM_INT_N_CS_PRECEDES,  &save_as_uint8    },
	{ _CITRUS_LC_MONETARY_SYM_INT_N_SEP_BY_SPACE, &save_as_uint8    },
	{ _CITRUS_LC_MONETARY_SYM_INT_P_SIGN_POSN,    &save_as_uint8    },
	{ _CITRUS_LC_MONETARY_SYM_INT_N_SIGN_POSN,    &save_as_uint8    },
	{ NULL },
    },
};

static const category_t lc_numeric = {
    _CITRUS_LC_NUMERIC_MAGIC_1,
    _CITRUS_LC_NUMERIC_SYM_VERSION,
    _CITRUS_LC_NUMERIC_VERSION,
    {
	{ _CITRUS_LC_NUMERIC_SYM_DECIMAL_POINT, &save_as_string   },
	{ _CITRUS_LC_NUMERIC_SYM_THOUSANDS_SEP, &save_as_string   },
	{ _CITRUS_LC_NUMERIC_SYM_GROUPING,      &save_as_grouping },
	{ NULL },
    },
};

static const category_t lc_time = {
    _CITRUS_LC_TIME_MAGIC_1,
    _CITRUS_LC_TIME_SYM_VERSION,
    _CITRUS_LC_TIME_VERSION,
    {
	{ _CITRUS_LC_TIME_SYM_ABDAY_1,     &save_as_string },
	{ _CITRUS_LC_TIME_SYM_ABDAY_2,     &save_as_string },
	{ _CITRUS_LC_TIME_SYM_ABDAY_3,     &save_as_string },
	{ _CITRUS_LC_TIME_SYM_ABDAY_4,     &save_as_string },
	{ _CITRUS_LC_TIME_SYM_ABDAY_5,     &save_as_string },
	{ _CITRUS_LC_TIME_SYM_ABDAY_6,     &save_as_string },
	{ _CITRUS_LC_TIME_SYM_ABDAY_7,     &save_as_string },
	{ _CITRUS_LC_TIME_SYM_DAY_1,       &save_as_string },
	{ _CITRUS_LC_TIME_SYM_DAY_2,       &save_as_string },
	{ _CITRUS_LC_TIME_SYM_DAY_3,       &save_as_string },
	{ _CITRUS_LC_TIME_SYM_DAY_4,       &save_as_string },
	{ _CITRUS_LC_TIME_SYM_DAY_5,       &save_as_string },
	{ _CITRUS_LC_TIME_SYM_DAY_6,       &save_as_string },
	{ _CITRUS_LC_TIME_SYM_DAY_7,       &save_as_string },
	{ _CITRUS_LC_TIME_SYM_ABMON_1,     &save_as_string },
	{ _CITRUS_LC_TIME_SYM_ABMON_2,     &save_as_string },
	{ _CITRUS_LC_TIME_SYM_ABMON_3,     &save_as_string },
	{ _CITRUS_LC_TIME_SYM_ABMON_4,     &save_as_string },
	{ _CITRUS_LC_TIME_SYM_ABMON_5,     &save_as_string },
	{ _CITRUS_LC_TIME_SYM_ABMON_6,     &save_as_string },
	{ _CITRUS_LC_TIME_SYM_ABMON_7,     &save_as_string },
	{ _CITRUS_LC_TIME_SYM_ABMON_8,     &save_as_string },
	{ _CITRUS_LC_TIME_SYM_ABMON_9,     &save_as_string },
	{ _CITRUS_LC_TIME_SYM_ABMON_10,    &save_as_string },
	{ _CITRUS_LC_TIME_SYM_ABMON_11,    &save_as_string },
	{ _CITRUS_LC_TIME_SYM_ABMON_12,    &save_as_string },
	{ _CITRUS_LC_TIME_SYM_MON_1,       &save_as_string },
	{ _CITRUS_LC_TIME_SYM_MON_2,       &save_as_string },
	{ _CITRUS_LC_TIME_SYM_MON_3,       &save_as_string },
	{ _CITRUS_LC_TIME_SYM_MON_4,       &save_as_string },
	{ _CITRUS_LC_TIME_SYM_MON_5,       &save_as_string },
	{ _CITRUS_LC_TIME_SYM_MON_6,       &save_as_string },
	{ _CITRUS_LC_TIME_SYM_MON_7,       &save_as_string },
	{ _CITRUS_LC_TIME_SYM_MON_8,       &save_as_string },
	{ _CITRUS_LC_TIME_SYM_MON_9,       &save_as_string },
	{ _CITRUS_LC_TIME_SYM_MON_10,      &save_as_string },
	{ _CITRUS_LC_TIME_SYM_MON_11,      &save_as_string },
	{ _CITRUS_LC_TIME_SYM_MON_12,      &save_as_string },
	{ _CITRUS_LC_TIME_SYM_AM_STR,      &save_as_string },
	{ _CITRUS_LC_TIME_SYM_PM_STR,      &save_as_string },
	{ _CITRUS_LC_TIME_SYM_D_T_FMT,     &save_as_string },
	{ _CITRUS_LC_TIME_SYM_D_FMT,       &save_as_string },
	{ _CITRUS_LC_TIME_SYM_T_FMT,       &save_as_string },
	{ _CITRUS_LC_TIME_SYM_T_FMT_AMPM,  &save_as_string },
	{ NULL },
    },
};

static const category_t lc_messages = {
    _CITRUS_LC_MESSAGES_MAGIC_1,
    _CITRUS_LC_MESSAGES_SYM_VERSION,
    _CITRUS_LC_MESSAGES_VERSION,
    {
	{ _CITRUS_LC_MESSAGES_SYM_YESEXPR, &save_as_string },
	{ _CITRUS_LC_MESSAGES_SYM_NOEXPR,  &save_as_string },
	{ _CITRUS_LC_MESSAGES_SYM_YESSTR,  &save_as_string },
	{ _CITRUS_LC_MESSAGES_SYM_NOSTR,   &save_as_string },
	{ NULL },
    },
};

void
mklocaledb(const char *type, FILE *reader, FILE *writer)
{
	static const char delim[3] = { '\0', '\0', '#' };
	const category_t *category = NULL;
	struct _db_factory *df;
	const token_t *token;
	char *line;
	size_t size;
	void *serialized;
	struct _region r;

	_DIAGASSERT(type != NULL);
	_DIAGASSERT(reader != NULL);
	_DIAGASSERT(writer != NULL);

	if (!strcasecmp(type, "MONETARY"))
		category = &lc_monetary;
	else if (!strcasecmp(type, "NUMERIC"))
		category = &lc_numeric;
	else if (!strcasecmp(type, "TIME"))
		category = &lc_time;
	else if (!strcasecmp(type, "MESSAGES"))
		category = &lc_messages;
	else {
		usage();
		/*NOTREACHED*/
	}
	if (_db_factory_create(&df, &_db_hash_std, NULL))
		errx(EXIT_FAILURE, "can't create db factory");
	if (_db_factory_add32_by_s(df, category->vers_sym, category->version))
		errx(EXIT_FAILURE, "can't store db");
	token = &category->tokens[0];
	while (token->key != NULL) {
		line = fparseln(reader, NULL,
		    NULL, delim, FPARSELN_UNESCALL);
		if (line == NULL)
			errx(EXIT_FAILURE, "can't read line");
		if ((*token->save)(df, token->key, (const char *)line))
			errx(EXIT_FAILURE, "can't store db");
		free(line);
		++token;
	}
	size = _db_factory_calc_size(df);
	_DIAGASSERT(size > 0);
	serialized = malloc(size);
	if (serialized == NULL)
		errx(EXIT_FAILURE, "can't malloc");
	_DIAGASSERT(serialized != NULL);
	_region_init(&r, serialized, size);
	if (_db_factory_serialize(df, category->magic, &r))
		errx(EXIT_FAILURE, "can't serialize db");
	fwrite(serialized, size, 1, writer);
	_db_factory_free(df);
}
