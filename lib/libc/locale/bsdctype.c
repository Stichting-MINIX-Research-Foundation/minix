/* $NetBSD: bsdctype.c,v 1.9 2010/06/20 02:23:15 tnozaki Exp $ */

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
__RCSID("$NetBSD: bsdctype.c,v 1.9 2010/06/20 02:23:15 tnozaki Exp $");
#endif /* LIBC_SCCS and not lint */

#include <sys/endian.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bsdctype_local.h"
#include "runetype_misc.h"

const _BSDCTypeLocale _DefaultBSDCTypeLocale = {
    _C_ctype_,
    _C_tolower_,
    _C_toupper_
};

const _BSDCTypeLocale *_CurrentBSDCTypeLocale = &_DefaultBSDCTypeLocale;

typedef struct {
	_BSDCTypeLocale	bl;
	unsigned char	blp_ctype_tab  [_CTYPE_NUM_CHARS + 1];
	short		blp_tolower_tab[_CTYPE_NUM_CHARS + 1];
	short		blp_toupper_tab[_CTYPE_NUM_CHARS + 1];
} _BSDCTypeLocalePriv;

static __inline void
_bsdctype_init_priv(_BSDCTypeLocalePriv *blp)
{
#if _CTYPE_CACHE_SIZE != _CTYPE_NUM_CHARS
	int i;

	for (i = _CTYPE_CACHE_SIZE; i < _CTYPE_NUM_CHARS; ++i) {
		blp->blp_ctype_tab  [i + 1] = 0;
		blp->blp_tolower_tab[i + 1] = i;
		blp->blp_toupper_tab[i + 1] = i;
	}
#endif
	blp->blp_ctype_tab  [0] = 0;
	blp->blp_tolower_tab[0] = EOF;
	blp->blp_toupper_tab[0] = EOF;
	blp->bl.bl_ctype_tab   = &blp->blp_ctype_tab  [0];
	blp->bl.bl_tolower_tab = &blp->blp_tolower_tab[0];
	blp->bl.bl_toupper_tab = &blp->blp_toupper_tab[0];
}

static __inline int
_bsdctype_read_file(const char * __restrict var, size_t lenvar,
    _BSDCTypeLocalePriv * __restrict blp)
{
	const _FileBSDCTypeLocale *fbl;
	uint32_t value;
	int i;

	_DIAGASSERT(blp != NULL);

	if (lenvar < sizeof(*fbl))
		return EFTYPE;
	fbl = (const _FileBSDCTypeLocale *)(const void *)var;
	if (memcmp(&fbl->fbl_id[0], _CTYPE_ID, sizeof(fbl->fbl_id)))
		return EFTYPE;
	value = be32toh(fbl->fbl_rev);
	if (value != _CTYPE_REV)
		return EFTYPE;
	value = be32toh(fbl->fbl_num_chars);
	if (value != _CTYPE_CACHE_SIZE)
		return EFTYPE;
	for (i = 0; i < _CTYPE_CACHE_SIZE; ++i) {
		blp->blp_ctype_tab  [i + 1] = fbl->fbl_ctype_tab[i];
		blp->blp_tolower_tab[i + 1] = be16toh(fbl->fbl_tolower_tab[i]);
		blp->blp_toupper_tab[i + 1] = be16toh(fbl->fbl_toupper_tab[i]);
	}
	return 0;
}

static __inline int
_bsdctype_read_runetype(const char * __restrict var, size_t lenvar,
    _BSDCTypeLocalePriv * __restrict blp)
{
	const _FileRuneLocale *frl;
	int i;

	_DIAGASSERT(blp != NULL);

	if (lenvar < sizeof(*frl))
		return EFTYPE;
	lenvar -= sizeof(*frl);
	frl = (const _FileRuneLocale *)(const void *)var;
	if (memcmp(_RUNECT10_MAGIC, &frl->frl_magic[0], sizeof(frl->frl_magic)))
		return EFTYPE;
	if (frl->frl_encoding[0] != 'N' || frl->frl_encoding[1] != 'O' ||
	    frl->frl_encoding[2] != 'N' || frl->frl_encoding[3] != 'E' ||
	    frl->frl_encoding[4] != '\0') /* XXX */
		return EFTYPE;
	if (be32toh(frl->frl_runetype_ext.frr_nranges) != 0 ||
	    be32toh(frl->frl_maplower_ext.frr_nranges) != 0 ||
	    be32toh(frl->frl_mapupper_ext.frr_nranges) != 0)
		return EFTYPE;
	if (lenvar < be32toh((uint32_t)frl->frl_variable_len))
		return EFTYPE;
	for (i = 0; i < _CTYPE_CACHE_SIZE; ++i) {
		blp->blp_ctype_tab  [i + 1] = (unsigned char)
		    _runetype_to_ctype((_RuneType)
		    be32toh(frl->frl_runetype[i]));
		blp->blp_tolower_tab[i + 1] = (short)
		    be32toh((uint32_t)frl->frl_maplower[i]);
		blp->blp_toupper_tab[i + 1] = (short)
		    be32toh((uint32_t)frl->frl_mapupper[i]);
	}
	return 0;
}

int
_bsdctype_load(const char * __restrict var, size_t lenvar,
    _BSDCTypeLocale ** __restrict pbl)
{
	int ret;
	_BSDCTypeLocalePriv *blp;

	_DIAGASSERT(var != NULL || lenvar < 1);
	_DIAGASSERT(pbl != NULL);

	if (lenvar < 1)
		return EFTYPE;
	blp = malloc(sizeof(*blp));
	if (blp == NULL)
		return errno;
	_bsdctype_init_priv(blp);
	switch (*var) {
	case 'B':
		_bsdctype_read_file(var, lenvar, blp);
		break;
	case 'R':
		_bsdctype_read_runetype(var, lenvar, blp);
		break;
	default:
		ret = EFTYPE;
	}
	if (ret)
		free(blp);
	else
		*pbl = &blp->bl;
	return ret;
}
