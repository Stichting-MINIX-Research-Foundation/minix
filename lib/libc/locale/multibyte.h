/*	$NetBSD: multibyte.h,v 1.6 2013/08/18 20:03:48 joerg Exp $	*/

/*-
 * Copyright (c)2002 Citrus Project,
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

#ifndef _MULTIBYTE_H_
#define _MULTIBYTE_H_

/* mbstate_t private */

#ifdef _BSD_MBSTATE_T_
typedef	_BSD_MBSTATE_T_	mbstate_t;
#undef _BSD_MBSTATE_T_
#endif

typedef struct _RuneStatePriv {
	_RuneLocale	*__runelocale;
	char		__private __attribute__((__aligned__));
} _RuneStatePriv;

typedef union _RuneState {
	mbstate_t		__pad;
	struct _RuneStatePriv	__priv;
#define rs_runelocale		__priv.__runelocale
#define rs_private		__priv.__private
} _RuneState;
#define _PRIVSIZE	(sizeof(mbstate_t)-offsetof(_RuneStatePriv, __private))

#define _RUNE_LOCALE(loc) \
    ((_RuneLocale *)((loc)->part_impl[(size_t)LC_CTYPE]))

#define _CITRUS_CTYPE(loc) \
    (((_RuneLocale *)((loc)->part_impl[(size_t)LC_CTYPE]))->rl_citrus_ctype)

/* */

static __inline _RuneState *
_ps_to_runestate(mbstate_t *ps)
{
	return (_RuneState *)(void *)ps;
}

static __inline _RuneState const *
_ps_to_runestate_const(mbstate_t const *ps)
{
	return (_RuneState const *)(void const *)ps;
}

static __inline _RuneLocale *
_ps_to_runelocale(mbstate_t const *ps)
{
	return _ps_to_runestate_const(ps)->rs_runelocale;
}

static __inline _citrus_ctype_t
_ps_to_ctype(mbstate_t const *ps, locale_t loc)
{
	if (!ps)
		return _CITRUS_CTYPE(loc);

	_DIAGASSERT(_ps_to_runelocale(ps) != NULL);

	return _ps_to_runelocale(ps)->rl_citrus_ctype;
}

static __inline void *
_ps_to_private(mbstate_t *ps)
{
	if (ps == NULL)
		return NULL;
	return (void *)&_ps_to_runestate(ps)->rs_private;
}

static __inline void const *
_ps_to_private_const(mbstate_t const *ps)
{
	if (ps == NULL)
		return NULL;
	return (void const *)&_ps_to_runestate_const(ps)->rs_private;
}

static __inline void
_init_ps(_RuneLocale *rl, mbstate_t *ps)
{
	size_t dum;
	_ps_to_runestate(ps)->rs_runelocale = rl;
	_citrus_ctype_mbrtowc(rl->rl_citrus_ctype, NULL, NULL, 0,
			      _ps_to_private(ps), &dum);
}

static __inline void
_fixup_ps(_RuneLocale *rl, mbstate_t *ps, int forceinit)
{
	/* for future multi-locale facility */
	_DIAGASSERT(rl != NULL);

	if (ps != NULL && (_ps_to_runelocale(ps) == NULL || forceinit)) {
		_init_ps(rl, ps);
	}
}

#endif /*_MULTIBYTE_H_*/
