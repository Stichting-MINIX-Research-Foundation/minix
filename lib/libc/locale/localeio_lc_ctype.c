/* $NetBSD: localeio_lc_ctype.c,v 1.6 2010/06/19 13:26:52 tnozaki Exp $ */

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
__RCSID("$NetBSD: localeio_lc_ctype.c,v 1.6 2010/06/19 13:26:52 tnozaki Exp $");
#endif /* LIBC_SCCS and not lint */

#include "reentrant.h"
#include <sys/types.h>
#include <sys/ctype_bits.h>
#include <sys/queue.h>
#include <assert.h>
#include <errno.h>
#include <langinfo.h>
#include <limits.h>
#define __SETLOCALE_SOURCE__
#include <locale.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsdctype_local.h"
#include "aliasname_local.h"
#include "localeio.h"

#include "setlocale_local.h"

/*
 * macro required by all template headers
 */
#define _PREFIX(name)		__CONCAT(_localeio_LC_CTYPE_, name)

/*
 * macro required by nb_lc_template(_decl).h
 */
#define _CATEGORY_TYPE		_BSDCTypeLocale

#include "nb_lc_template_decl.h"

static int
/*ARGSUSED*/
_localeio_LC_CTYPE_create_impl(const char * __restrict root,
    const char * __restrict name, _BSDCTypeLocale ** __restrict pdata)
{
	char path[PATH_MAX + 1];
	void *var;
	size_t lenvar;
	int ret;

	_DIAGASSERT(root != NULL);
	_DIAGASSERT(name != NULL);
	_DIAGASSERT(pdata != NULL);

	snprintf(path, sizeof(path),
	    "%s/%s/LC_CTYPE", root, name);
	ret = _localeio_map_file(path, &var, &lenvar);
	if (!ret) {
		ret = _bsdctype_load((const char *)var, lenvar, pdata);
		_localeio_unmap_file(var, lenvar);
	}
	return ret;
}

static __inline void
_PREFIX(build_cache)(struct _locale_cache_t * __restrict cache,
    _BSDCTypeLocale * __restrict data)
{
	_DIAGASSERT(cache != NULL);
	_DIAGASSERT(data != NULL);

	cache->ctype_tab   = data->bl_ctype_tab;
	cache->tolower_tab = data->bl_tolower_tab;
	cache->toupper_tab = data->bl_toupper_tab;
	cache->mb_cur_max  = (size_t)1;
}

static __inline void
_PREFIX(fixup)(_BSDCTypeLocale *data)
{
	_DIAGASSERT(data != NULL);

	_ctype_       = data->bl_ctype_tab;
	_tolower_tab_ = data->bl_tolower_tab;
	_toupper_tab_ = data->bl_toupper_tab;
}

/*
 * macro required by nb_lc_template.h
 */
#define _CATEGORY_ID		LC_CTYPE
#define _CATEGORY_NAME		"LC_CTYPE"
#define _CATEGORY_DEFAULT	_DefaultBSDCTypeLocale

#include "nb_lc_template.h"
#include "generic_lc_template.h"
_LOCALE_CATEGORY_ENTRY(_localeio_LC_CTYPE_);
