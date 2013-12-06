/* $NetBSD: global_locale.c,v 1.21 2013/09/13 13:13:32 joerg Exp $ */

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
__RCSID("$NetBSD: global_locale.c,v 1.21 2013/09/13 13:13:32 joerg Exp $");
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/ctype_bits.h>
#include <sys/localedef.h>
#include <langinfo.h>
#include <limits.h>
#define __SETLOCALE_SOURCE__
#include <locale.h>
#include <stdlib.h>

#include "runetype_local.h"
#include "setlocale_local.h"

#ifndef NBCHAR_MAX
#define NBCHAR_MAX (char)CHAR_MAX
#endif

static const _MessagesLocale _DefaultMessagesLocale = {
	"^[Yy]",
	"^[Nn]",
	"yes",
	"no"
};

static const _MonetaryLocale _DefaultMonetaryLocale = {
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	(char)CHAR_MAX,
	(char)CHAR_MAX,
	(char)CHAR_MAX,
	(char)CHAR_MAX,
	(char)CHAR_MAX,
	(char)CHAR_MAX,
	(char)CHAR_MAX,
	(char)CHAR_MAX,
	(char)CHAR_MAX,
	(char)CHAR_MAX,
	(char)CHAR_MAX,
	(char)CHAR_MAX,
	(char)CHAR_MAX,
	(char)CHAR_MAX
};

static const _NumericLocale _DefaultNumericLocale = {
	".",
	"",
	""
};

static const _TimeLocale _DefaultTimeLocale = 
{
	{
		"Sun","Mon","Tue","Wed","Thu","Fri","Sat",
	},
	{
		"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday",
		"Friday", "Saturday"
	},
	{
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	},
	{
		"January", "February", "March", "April", "May", "June", "July",
		"August", "September", "October", "November", "December"
	},
	{
		"AM", "PM"
	},
	"%a %b %e %H:%M:%S %Y",
	"%m/%d/%y",
	"%H:%M:%S",
	"%I:%M:%S %p"
};

static const char _lc_C_locale_name[] = _C_LOCALE;

__dso_hidden const struct _locale_cache_t _C_cache = {
    .ldata = {
	.decimal_point		= __UNCONST("."),
	.thousands_sep		= __UNCONST(""),
	.grouping		= __UNCONST(""),
	.int_curr_symbol	= __UNCONST(""),
	.currency_symbol	= __UNCONST(""),
	.mon_decimal_point	= __UNCONST(""),
	.mon_thousands_sep	= __UNCONST(""),
	.mon_grouping		= __UNCONST(""),
	.positive_sign		= __UNCONST(""),
	.negative_sign		= __UNCONST(""),
	.int_frac_digits	= NBCHAR_MAX,
	.frac_digits		= NBCHAR_MAX,
	.p_cs_precedes		= NBCHAR_MAX,
	.p_sep_by_space		= NBCHAR_MAX,
	.n_cs_precedes		= NBCHAR_MAX,
	.n_sep_by_space		= NBCHAR_MAX,
	.p_sign_posn		= NBCHAR_MAX,
	.n_sign_posn		= NBCHAR_MAX,
	.int_p_cs_precedes	= NBCHAR_MAX,
	.int_n_cs_precedes	= NBCHAR_MAX,
	.int_p_sep_by_space	= NBCHAR_MAX,
	.int_n_sep_by_space	= NBCHAR_MAX,
	.int_p_sign_posn	= NBCHAR_MAX,
	.int_n_sign_posn	= NBCHAR_MAX,
    },
    .monetary_name = _lc_C_locale_name,
    .numeric_name = _lc_C_locale_name,
};

__dso_protected struct _locale _lc_global_locale = {
    .cache = &_C_cache,
    .query = { _C_LOCALE },
    .part_name = {
	[(size_t)LC_ALL     ] = _lc_C_locale_name,
	[(size_t)LC_COLLATE ] = _lc_C_locale_name,
	[(size_t)LC_CTYPE   ] = _lc_C_locale_name,
	[(size_t)LC_MONETARY] = _lc_C_locale_name,
	[(size_t)LC_NUMERIC ] = _lc_C_locale_name,
	[(size_t)LC_TIME    ] = _lc_C_locale_name,
	[(size_t)LC_MESSAGES] = _lc_C_locale_name,
    },
    .part_impl = {
	[(size_t)LC_ALL     ] = (_locale_part_t)NULL,
	[(size_t)LC_COLLATE ] = (_locale_part_t)NULL,
	[(size_t)LC_CTYPE   ] = (_locale_part_t)
	    __UNCONST(&_DefaultRuneLocale),
	[(size_t)LC_MONETARY] = (_locale_part_t)
	    __UNCONST(&_DefaultMonetaryLocale),
	[(size_t)LC_NUMERIC ] = (_locale_part_t)
	    __UNCONST(&_DefaultNumericLocale),
	[(size_t)LC_MESSAGES] = (_locale_part_t)
	    __UNCONST(&_DefaultMessagesLocale),
	[(size_t)LC_TIME] = (_locale_part_t)
	    __UNCONST(&_DefaultTimeLocale),
    },
};

__dso_protected const struct _locale _lc_C_locale = {
    .cache = &_C_cache,
    .query = { _C_LOCALE },
    .part_name = {
	[(size_t)LC_ALL     ] = _lc_C_locale_name,
	[(size_t)LC_COLLATE ] = _lc_C_locale_name,
	[(size_t)LC_CTYPE   ] = _lc_C_locale_name,
	[(size_t)LC_MONETARY] = _lc_C_locale_name,
	[(size_t)LC_NUMERIC ] = _lc_C_locale_name,
	[(size_t)LC_TIME    ] = _lc_C_locale_name,
	[(size_t)LC_MESSAGES] = _lc_C_locale_name,
    },
    .part_impl = {
	[(size_t)LC_ALL     ] = (_locale_part_t)NULL,
	[(size_t)LC_COLLATE ] = (_locale_part_t)NULL,
	[(size_t)LC_CTYPE   ] = (_locale_part_t)
	    __UNCONST(&_DefaultRuneLocale),
	[(size_t)LC_MONETARY] = (_locale_part_t)
	    __UNCONST(&_DefaultMonetaryLocale),
	[(size_t)LC_NUMERIC ] = (_locale_part_t)
	    __UNCONST(&_DefaultNumericLocale),
	[(size_t)LC_MESSAGES] = (_locale_part_t)
	    __UNCONST(&_DefaultMessagesLocale),
	[(size_t)LC_TIME] = (_locale_part_t)
	    __UNCONST(&_DefaultTimeLocale),
    },
};
