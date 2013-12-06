/* $NetBSD: dummy_lc_template.h,v 1.4 2013/04/14 23:30:16 joerg Exp $ */

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

#ifndef _DUMMY_LC_TEMPLATE_H_
#define _DUMMY_LC_TEMPLATE_H_

#include "generic_lc_template_decl.h"

const char *
_PREFIX(setlocale)(const char * __restrict name,
    struct _locale * __restrict locale)
{
	if (name != NULL) {
		if (*name == '\0')
			name = _get_locale_env(_CATEGORY_NAME);
		if (strcmp(name, locale->part_name[(size_t)_CATEGORY_ID])) {
			if (!strcmp(_C_LOCALE, name))
				name = _C_LOCALE;
			else if (!strcmp(_POSIX_LOCALE, name))
				name = _POSIX_LOCALE;
			else
				return NULL;
			locale->part_name[(size_t)_CATEGORY_ID] = name;
		}
	}
	return locale->part_name[(size_t)_CATEGORY_ID];
}

#endif /*_DUMMY_LC_TEMPLATE_H_*/
