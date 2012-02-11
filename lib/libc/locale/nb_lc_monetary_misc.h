/* $NetBSD: nb_lc_monetary_misc.h,v 1.3 2010/03/27 15:25:22 tnozaki Exp $ */

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

#ifndef _NB_LC_MONETARY_MISC_H_
#define _NB_LC_MONETARY_MISC_H_

/*
 * macro required by nb_lc_template(_decl).h
 */
#define _CATEGORY_TYPE		_MonetaryLocale

static __inline void
_PREFIX(build_cache)(struct _locale_cache_t * __restrict cache,
    _MonetaryLocale * __restrict data)
{
	struct lconv *ldata;

	_DIAGASSERT(cache != NULL);
	_DIAGASSERT(cache->ldata != NULL);
	_DIAGASSERT(cache->items != NULL);
	_DIAGASSERT(data != NULL);

	ldata = cache->ldata;
	ldata->int_curr_symbol   = __UNCONST(data->int_curr_symbol);
	ldata->currency_symbol   = __UNCONST(data->currency_symbol);
	ldata->mon_decimal_point = __UNCONST(data->mon_decimal_point);
	ldata->mon_thousands_sep = __UNCONST(data->mon_thousands_sep);
	ldata->mon_grouping      = __UNCONST(data->mon_grouping);
	ldata->positive_sign     = __UNCONST(data->positive_sign);
	ldata->negative_sign     = __UNCONST(data->negative_sign);

	ldata->int_frac_digits    = data->int_frac_digits;
	ldata->frac_digits        = data->frac_digits;
	ldata->p_cs_precedes      = data->p_cs_precedes;
	ldata->p_sep_by_space     = data->p_sep_by_space;
	ldata->n_cs_precedes      = data->n_cs_precedes;
	ldata->n_sep_by_space     = data->n_sep_by_space;
	ldata->p_sign_posn        = data->p_sign_posn;
	ldata->n_sign_posn        = data->n_sign_posn;
	ldata->int_p_cs_precedes  = data->int_p_cs_precedes;
	ldata->int_n_cs_precedes  = data->int_n_cs_precedes;
	ldata->int_p_sep_by_space = data-> int_p_sep_by_space;
	ldata->int_n_sep_by_space = data->int_n_sep_by_space;
	ldata->int_p_sign_posn    = data->int_p_sign_posn;
	ldata->int_n_sign_posn    = data->int_n_sign_posn;

	cache->items[(size_t)CRNCYSTR] = NULL; /* NOT IMPLEMENTED YET */
}

static __inline void
_PREFIX(fixup)(_MonetaryLocale *data)
{
	_DIAGASSERT(data != NULL);

	_CurrentMonetaryLocale = data;
}

/*
 * macro required by nb_lc_template.h
 */
#define _CATEGORY_ID		LC_MONETARY
#define _CATEGORY_NAME		"LC_MONETARY"
#define _CATEGORY_DEFAULT	_DefaultMonetaryLocale

#endif /*_RUNE_LC_MONETARY_MISC_H_*/
