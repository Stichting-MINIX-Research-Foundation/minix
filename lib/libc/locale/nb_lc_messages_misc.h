/* $NetBSD: nb_lc_messages_misc.h,v 1.3 2010/03/27 15:25:22 tnozaki Exp $ */

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

#ifndef _NB_LC_MESSAGES_MISC_H_
#define _NB_LC_MESSAGES_MISC_H_

/*
 * macro required by nb_lc_template(_decl).h
 */
#define _CATEGORY_TYPE		_MessagesLocale

static __inline void
_PREFIX(build_cache)(struct _locale_cache_t * __restrict cache,
    _MessagesLocale * __restrict data)
{
	_DIAGASSERT(cache != NULL);
	_DIAGASSERT(cache->items != NULL);
	_DIAGASSERT(data != NULL);

	cache->items[(size_t)YESSTR ] = data->yesstr;
	cache->items[(size_t)YESEXPR] = data->yesexpr;
	cache->items[(size_t)NOSTR  ] = data->nostr;
	cache->items[(size_t)NOEXPR ] = data->noexpr;
}

static __inline void
_PREFIX(fixup)(_MessagesLocale *data)
{
	_DIAGASSERT(data != NULL);

	_CurrentMessagesLocale = data;
}

/*
 * macro required by nb_lc_template.h
 */
#define _CATEGORY_ID		LC_MESSAGES
#define _CATEGORY_NAME		"LC_MESSAGES"
#define _CATEGORY_DEFAULT	_DefaultMessagesLocale

#endif /*_NB_LC_MESSAGES_MISC_H_*/
