/* $NetBSD: citrus_lc_ctype.c,v 1.15 2013/09/13 13:13:32 joerg Exp $ */

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
__RCSID("$NetBSD: citrus_lc_ctype.c,v 1.15 2013/09/13 13:13:32 joerg Exp $");
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
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "setlocale_local.h"

#include "citrus_namespace.h"
#include "citrus_bcs.h"
#include "citrus_region.h"
#include "citrus_lookup.h"
#include "citrus_aliasname_local.h"
#include "citrus_module.h"
#include "citrus_ctype.h"
#include "citrus_mmap.h"

#include "runetype_local.h"
#include "multibyte.h"

/*
 * macro required by all template headers
 */
#define _PREFIX(name)		__CONCAT(_citrus_LC_CTYPE_, name)

/*
 * macro required by nb_lc_template(_decl).h
 */
#define _CATEGORY_TYPE          _RuneLocale

#include "nb_lc_template_decl.h"

static int
_citrus_LC_CTYPE_create_impl(const char * __restrict root,
    const char * __restrict name, _RuneLocale ** __restrict pdata)
{
	char path[PATH_MAX + 1];
	int ret;
	struct _region r;

	_DIAGASSERT(root != NULL);
	_DIAGASSERT(name != NULL);
	_DIAGASSERT(pdata != NULL);

	snprintf(path, sizeof(path),
	    "%s/%s/LC_CTYPE", root, name);
	ret = _citrus_map_file(&r, path);
	if (!ret) {
		ret = _rune_load((const char *)r.r_head, r.r_size, pdata);
		_citrus_unmap_file(&r);
	}
	return ret;
}

static __inline void
_PREFIX(update_global)(_RuneLocale *data)
{
	_DIAGASSERT(data != NULL);

	__mb_cur_max = _citrus_ctype_get_mb_cur_max(data->rl_citrus_ctype);
	_ctype_tab_ = data->rl_ctype_tab;
	_tolower_tab_ = data->rl_tolower_tab;
	_toupper_tab_ = data->rl_toupper_tab;

#ifdef __BUILD_LEGACY
	_ctype_ = data->rl_compat_bsdctype;
#endif
}

/*
 * macro required by nb_lc_template.h
 */
#define _CATEGORY_ID		LC_CTYPE
#define _CATEGORY_NAME		"LC_CTYPE"

#include "nb_lc_template.h"
