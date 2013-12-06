/* $NetBSD: nb_lc_time_misc.h,v 1.8 2013/09/13 13:13:32 joerg Exp $ */

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

#ifndef _NB_LC_TIME_MISC_H_
#define _NB_LC_TIME_MISC_H_

/*
 * macro required by nb_lc_template(_decl).h
 */
#define _CATEGORY_TYPE		_TimeLocale

#define ABDAY_IDX(idx)	((size_t)idx - (size_t)ABDAY_1)
#define DAY_IDX(idx)	((size_t)idx - (size_t)DAY_1)
#define ABMON_IDX(idx)	((size_t)idx - (size_t)ABMON_1)
#define MON_IDX(idx)	((size_t)idx - (size_t)MON_1)
#define AM_PM_IDX(idx)	((size_t)idx - (size_t)AM_STR)

/* ARGSUSED */
static __inline void
_PREFIX(update_global)(_TimeLocale *data)
{
}

/*
 * macro required by nb_lc_template.h
 */
#define _CATEGORY_ID		LC_TIME
#define _CATEGORY_NAME		"LC_TIME"

#endif /*_NB_LC_TIME_MISC_H_*/
