/*	$NetBSD: citrus_ctype.c,v 1.7 2013/05/28 16:57:56 joerg Exp $	*/

/*-
 * Copyright (c)1999, 2000, 2001, 2002 Citrus Project,
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
__RCSID("$NetBSD: citrus_ctype.c,v 1.7 2013/05/28 16:57:56 joerg Exp $");
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <wchar.h>
#include "citrus_module.h"
#include "citrus_ctype.h"
#include "citrus_ctype_fallback.h"
#include "citrus_none.h"
#include _CITRUS_DEFAULT_CTYPE_HEADER

_citrus_ctype_rec_t _citrus_ctype_default = {
	&_CITRUS_DEFAULT_CTYPE_OPS,	/* cc_ops */
	NULL,				/* cc_closure */
	NULL				/* cc_module */
};

#ifdef _I18N_DYNAMIC

static int _initctypemodule(_citrus_ctype_t, char const *, _citrus_module_t,
			    void *, size_t, size_t);

static int
_initctypemodule(_citrus_ctype_t cc, char const *modname,
		 _citrus_module_t handle, void *variable, size_t lenvar,
		 size_t szpriv)
{
	int ret;
	_citrus_ctype_getops_t getops;

	_DIAGASSERT(cc != NULL);

	cc->cc_module = handle;

	getops = (_citrus_ctype_getops_t)_citrus_find_getops(cc->cc_module,
							     modname,
							     "ctype");
	if (getops == NULL)
		return (EINVAL);

	cc->cc_ops = (_citrus_ctype_ops_rec_t *)malloc(sizeof(*cc->cc_ops));
	if (cc->cc_ops == NULL)
		return (ENOMEM);

	ret = (*getops)(cc->cc_ops, sizeof(*cc->cc_ops),
			_CITRUS_CTYPE_ABI_VERSION);
	if (ret)
		goto bad;

	/* If return ABI version is not expected, fixup it here*/
	switch (cc->cc_ops->co_abi_version) {
	case 0x00000001:
		cc->cc_ops->co_btowc = &_citrus_ctype_btowc_fallback;
		cc->cc_ops->co_wctob = &_citrus_ctype_wctob_fallback;
		/* FALLTHROUGH */
	case 0x00000002:
		cc->cc_ops->co_mbsnrtowcs = &_citrus_ctype_mbsnrtowcs_fallback;
		cc->cc_ops->co_wcsnrtombs = &_citrus_ctype_wcsnrtombs_fallback;
		/* FALLTHROUGH */
	default:
		break;
	}

	/* validation check */
	if (cc->cc_ops->co_init == NULL ||
	    cc->cc_ops->co_uninit == NULL ||
	    cc->cc_ops->co_get_mb_cur_max == NULL ||
	    cc->cc_ops->co_mblen == NULL ||
	    cc->cc_ops->co_mbrlen == NULL ||
	    cc->cc_ops->co_mbrtowc == NULL ||
	    cc->cc_ops->co_mbsinit == NULL ||
	    cc->cc_ops->co_mbsrtowcs == NULL ||
	    cc->cc_ops->co_mbsnrtowcs == NULL ||
	    cc->cc_ops->co_mbstowcs == NULL ||
	    cc->cc_ops->co_mbtowc == NULL ||
	    cc->cc_ops->co_wcrtomb == NULL ||
	    cc->cc_ops->co_wcsrtombs == NULL ||
	    cc->cc_ops->co_wcsnrtombs == NULL ||
	    cc->cc_ops->co_wcstombs == NULL ||
	    cc->cc_ops->co_wctomb == NULL ||
	    cc->cc_ops->co_btowc == NULL ||
	    cc->cc_ops->co_wctob == NULL) {
		ret = EINVAL;
		goto bad;
	}

	/* init and get closure */
	ret = (*cc->cc_ops->co_init)(
		&cc->cc_closure, variable, lenvar, szpriv);
	if (ret)
		goto bad;

	return (0);

bad:
	if (cc->cc_ops)
		free(cc->cc_ops);
	cc->cc_ops = NULL;

	return (ret);
}

int
_citrus_ctype_open(_citrus_ctype_t *rcc,
		   char const *encname, void *variable, size_t lenvar,
		   size_t szpriv)
{
	int ret;
	_citrus_module_t handle;
	_citrus_ctype_t cc;

	_DIAGASSERT(encname != NULL);
	_DIAGASSERT(!lenvar || variable!=NULL);
	_DIAGASSERT(rcc != NULL);

	if (!strcmp(encname, _CITRUS_DEFAULT_CTYPE_NAME)) {
		*rcc = &_citrus_ctype_default;
		return (0);
	}
	ret = _citrus_load_module(&handle, encname);
	if (ret)
		return (ret);

	cc = calloc(1, sizeof(*cc));
	if (!cc) {
		_citrus_unload_module(handle);
		return (errno);
	}

	ret = _initctypemodule(cc, encname, handle, variable, lenvar, szpriv);
	if (ret) {
		_citrus_unload_module(cc->cc_module);
		free(cc);
		return (ret);
	}

	*rcc = cc;

	return (0);
}

void
_citrus_ctype_close(_citrus_ctype_t cc)
{

	_DIAGASSERT(cc != NULL);

	if (cc == &_citrus_ctype_default)
		return;
	(*cc->cc_ops->co_uninit)(cc->cc_closure);
	free(cc->cc_ops);
	_citrus_unload_module(cc->cc_module);
	free(cc);
}

#else
/* !_I18N_DYNAMIC */

int
/*ARGSUSED*/
_citrus_ctype_open(_citrus_ctype_t *rcc,
		   char const *encname, void *variable, size_t lenvar,
		   size_t szpriv)
{
	if (!strcmp(encname, _CITRUS_DEFAULT_CTYPE_NAME)) {
		*rcc = &_citrus_ctype_default;
		return (0);
	}
	return (EINVAL);
}

void
/*ARGSUSED*/
_citrus_ctype_close(_citrus_ctype_t cc)
{
}

#endif
