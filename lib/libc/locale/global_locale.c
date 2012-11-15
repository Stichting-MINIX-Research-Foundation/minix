/* $NetBSD: global_locale.c,v 1.13 2012/03/21 14:11:24 christos Exp $ */

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
__RCSID("$NetBSD: global_locale.c,v 1.13 2012/03/21 14:11:24 christos Exp $");
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

static struct lconv _global_ldata = {
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
};

static const char *_global_items[(size_t)ALT_DIGITS + 1] = {
	[(size_t)D_T_FMT    ] = "%a %b %e %H:%M:%S %Y",
	[(size_t)D_FMT      ] = "%m/%d/%y",
	[(size_t)T_FMT      ] = "%H:%M:%S",
	[(size_t)T_FMT_AMPM ] = "%I:%M:%S %p",
	[(size_t)AM_STR     ] = "AM",
	[(size_t)PM_STR     ] = "PM",
	[(size_t)DAY_1      ] = "Sun",
	[(size_t)DAY_2      ] = "Mon",
	[(size_t)DAY_3      ] = "Tue",
	[(size_t)DAY_4      ] = "Wed",
	[(size_t)DAY_5      ] = "Thu",
	[(size_t)DAY_6      ] = "Fri",
	[(size_t)DAY_7      ] = "Sat",
	[(size_t)ABDAY_1    ] = "Sunday",
	[(size_t)ABDAY_2    ] = "Monday",
	[(size_t)ABDAY_3    ] = "Tuesday",
	[(size_t)ABDAY_4    ] = "Wednesday",
	[(size_t)ABDAY_5    ] = "Thursday",
	[(size_t)ABDAY_6    ] = "Friday",
	[(size_t)ABDAY_7    ] = "Saturday",
	[(size_t)MON_1      ] = "Jan",
	[(size_t)MON_2      ] = "Feb",
	[(size_t)MON_3      ] = "Mar",
	[(size_t)MON_4      ] = "Apr",
	[(size_t)MON_5      ] = "May",
	[(size_t)MON_6      ] = "Jun",
	[(size_t)MON_7      ] = "Jul",
	[(size_t)MON_8      ] = "Aug",
	[(size_t)MON_9      ] = "Sep",
	[(size_t)MON_10     ] = "Oct",
	[(size_t)MON_11     ] = "Nov",
	[(size_t)MON_12     ] = "Dec",
	[(size_t)ABMON_1    ] = "January",
	[(size_t)ABMON_2    ] = "February",
	[(size_t)ABMON_3    ] = "March",
	[(size_t)ABMON_4    ] = "April",
	[(size_t)ABMON_5    ] = "May",
	[(size_t)ABMON_6    ] = "June",
	[(size_t)ABMON_7    ] = "July",
	[(size_t)ABMON_8    ] = "August",
	[(size_t)ABMON_9    ] = "September",
	[(size_t)ABMON_10   ] = "October",
	[(size_t)ABMON_11   ] = "November",
	[(size_t)ABMON_12   ] = "December",
	[(size_t)RADIXCHAR  ] = ".",
	[(size_t)THOUSEP    ] = "",
	[(size_t)YESSTR     ] = "yes",
	[(size_t)YESEXPR    ] = "^[Yy]",
	[(size_t)NOSTR      ] = "no",
	[(size_t)NOEXPR     ] = "^[Nn]",
	[(size_t)CRNCYSTR   ] = NULL,
	[(size_t)CODESET    ] = "646",
	[(size_t)ERA        ] = NULL,
	[(size_t)ERA_D_FMT  ] = NULL,
	[(size_t)ERA_D_T_FMT] = NULL,
	[(size_t)ERA_T_FMT  ] = NULL,
	[(size_t)ALT_DIGITS ] = NULL,
};

static struct _locale_cache_t _global_cache = {
    .ctype_tab   = (const unsigned char *)&_C_ctype_[0],
    .tolower_tab = (const short *)&_C_tolower_[0],
    .toupper_tab = (const short *)&_C_toupper_[0],
    .mb_cur_max = (size_t)1,
    .ldata = &_global_ldata,
    .items = &_global_items[0],
};

struct _locale_impl_t _global_locale = {
    .cache = &_global_cache,
    .query = { _C_LOCALE },
    .part_name = {
	[(size_t)LC_ALL     ] = _C_LOCALE,
	[(size_t)LC_COLLATE ] = _C_LOCALE,
	[(size_t)LC_CTYPE   ] = _C_LOCALE,
	[(size_t)LC_MONETARY] = _C_LOCALE,
	[(size_t)LC_NUMERIC ] = _C_LOCALE,
	[(size_t)LC_TIME    ] = _C_LOCALE,
	[(size_t)LC_MESSAGES] = _C_LOCALE,
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
    },
};
