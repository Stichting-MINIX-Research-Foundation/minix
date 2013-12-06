/* $NetBSD: setlocale_local.h,v 1.15 2013/09/13 13:13:32 joerg Exp $ */

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

#include <sys/queue.h>
#include <locale.h>

#include "ctype_local.h"

#define _LOCALENAME_LEN_MAX 33

#define _C_LOCALE		"C"
#define _POSIX_LOCALE		"POSIX"

extern const char		*_PathLocale;
#define _LOCALE_ALIAS_NAME	"locale.alias"

typedef void *_locale_part_t;

struct _locale_cache_t {
	SLIST_ENTRY(_locale_cache_t) cache_link;
	const char *monetary_name;
	const char *numeric_name;
	struct lconv ldata;
};

struct _locale {
	const struct _locale_cache_t *cache;
	char query[_LOCALENAME_LEN_MAX * (_LC_LAST - 1)];
	const char *part_name[_LC_LAST];
	_locale_part_t part_impl[_LC_LAST];
};

typedef const char *(*_locale_set_t)(const char * __restrict,
    struct _locale * __restrict);

__BEGIN_DECLS
_locale_set_t		_find_category(int);
const char		*_get_locale_env(const char *);
char			*__setlocale(int, const char *);

const char *_generic_LC_ALL_setlocale(
    const char * __restrict, struct _locale * __restrict);
const char *_dummy_LC_COLLATE_setlocale(
    const char * __restrict, struct _locale * __restrict);
const char *_citrus_LC_CTYPE_setlocale(
    const char * __restrict, struct _locale * __restrict);
const char *_citrus_LC_MONETARY_setlocale(
    const char * __restrict, struct _locale * __restrict);
const char *_citrus_LC_NUMERIC_setlocale(
    const char * __restrict, struct _locale * __restrict);
const char *_citrus_LC_TIME_setlocale(
    const char * __restrict, struct _locale * __restrict);
const char *_citrus_LC_MESSAGES_setlocale(
    const char * __restrict, struct _locale * __restrict);

int _setlocale_cache(locale_t, struct _locale_cache_t *);
__END_DECLS

#ifdef _LIBC
extern __dso_protected struct _locale	_lc_global_locale;
extern __dso_hidden const struct _locale_cache_t _C_cache;

static __inline struct _locale *
_current_locale(void)
{
	return &_lc_global_locale;
}
#endif

extern size_t __mb_len_max_runtime;

#endif /*_SETLOCALE_LOCAL_H_*/
