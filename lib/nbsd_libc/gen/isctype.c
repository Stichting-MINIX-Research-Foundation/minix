/* $NetBSD: isctype.c,v 1.21 2010/12/14 02:28:57 joerg Exp $ */

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
__RCSID("$NetBSD: isctype.c,v 1.21 2010/12/14 02:28:57 joerg Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/types.h>
#include <sys/ctype_bits.h>
#define _CTYPE_NOINLINE
#include <ctype.h>
#include <langinfo.h>
#define __SETLOCALE_SOURCE__
#include <locale.h>
#include <stdio.h>
#if EOF != -1
#error "EOF != -1"
#endif

#include "setlocale_local.h"

#define _CTYPE_TAB(table, i)	((_current_cache()->table + 1)[i])

#define _ISCTYPE_FUNC(name, bit) \
int \
is##name(int c) \
{ \
	return (int)(_CTYPE_TAB(ctype_tab, c) & (bit)); \
}

_ISCTYPE_FUNC(alnum,  _CTYPE_U|_CTYPE_L|_CTYPE_N      )
_ISCTYPE_FUNC(alpha,  _CTYPE_U|_CTYPE_L         )
_ISCTYPE_FUNC(cntrl,  _CTYPE_C            )
_ISCTYPE_FUNC(digit,  _CTYPE_N            )
_ISCTYPE_FUNC(graph,  _CTYPE_P|_CTYPE_U|_CTYPE_L|_CTYPE_N   )
_ISCTYPE_FUNC(lower,  _CTYPE_L            )
_ISCTYPE_FUNC(print,  _CTYPE_P|_CTYPE_U|_CTYPE_L|_CTYPE_N|_CTYPE_B)
_ISCTYPE_FUNC(punct,  _CTYPE_P            )
_ISCTYPE_FUNC(space,  _CTYPE_S            )
_ISCTYPE_FUNC(upper,  _CTYPE_U            )
_ISCTYPE_FUNC(xdigit, _CTYPE_N|_CTYPE_X         )

int
isblank(int c)
{
	/* XXX: FIXME */
        return c == ' ' || c == '\t';
}

int
toupper(int c)
{
	return (int)_CTYPE_TAB(toupper_tab, c);
}

int
tolower(int c)
{
	return (int)_CTYPE_TAB(tolower_tab, c);
}

int
_toupper(int c)
{
	return (c - 'a' + 'A');
}

int
_tolower(int c)
{
	return (c - 'A' + 'a');
}
