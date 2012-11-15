/* $NetBSD: setlocale_local.h,v 1.8 2012/03/04 21:14:57 tnozaki Exp $ */

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

#ifndef _SETLOCALE_LOCAL_H_
#define _SETLOCALE_LOCAL_H_

#define _LOCALENAME_LEN_MAX 33

#define _C_LOCALE		"C"
#define _POSIX_LOCALE		"POSIX"

extern const char		*_PathLocale;
#define _LOCALE_ALIAS_NAME	"locale.alias"

typedef void *_locale_part_t;

struct _locale_cache_t {
	const unsigned char *ctype_tab;
	const short *tolower_tab;
	const short *toupper_tab;
	size_t mb_cur_max;
	struct lconv *ldata;
	const char **items;
};

struct _locale_impl_t {
	struct _locale_cache_t *cache;
	char query[_LOCALENAME_LEN_MAX * (_LC_LAST - 1)];
	const char *part_name[_LC_LAST];
	_locale_part_t part_impl[_LC_LAST];
};

typedef const char *(*_locale_set_t)(const char * __restrict,
    struct _locale_impl_t * __restrict);

__BEGIN_DECLS
_locale_set_t		_find_category(int);
const char		*_get_locale_env(const char *);
struct _locale_impl_t	**_current_locale(void);
char			*__setlocale(int, const char *);

const char *_generic_LC_ALL_setlocale(
    const char * __restrict, struct _locale_impl_t * __restrict);
const char *_dummy_LC_COLLATE_setlocale(
    const char * __restrict, struct _locale_impl_t * __restrict);
const char *_citrus_LC_CTYPE_setlocale(
    const char * __restrict, struct _locale_impl_t * __restrict);
const char *_citrus_LC_MONETARY_setlocale(
    const char * __restrict, struct _locale_impl_t * __restrict);
const char *_citrus_LC_NUMERIC_setlocale(
    const char * __restrict, struct _locale_impl_t * __restrict);
const char *_citrus_LC_TIME_setlocale(
    const char * __restrict, struct _locale_impl_t * __restrict);
const char *_citrus_LC_MESSAGES_setlocale(
    const char * __restrict, struct _locale_impl_t * __restrict);
__END_DECLS

static __inline struct _locale_cache_t *
_current_cache(void)
{
	return (*_current_locale())->cache;
}

extern struct _locale_impl_t	_global_locale;
extern size_t __mb_len_max_runtime;

#endif /*_SETLOCALE_LOCAL_H_*/
