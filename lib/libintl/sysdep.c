/*	$NetBSD: sysdep.c,v 1.2 2005/04/27 09:50:26 yamt Exp $	*/

/*-
 * Copyright (c)2004 Citrus Project,
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
__RCSID("$NetBSD: sysdep.c,v 1.2 2005/04/27 09:50:26 yamt Exp $");
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/types.h>

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "libintl_local.h"

struct sysdep_pair
{
	const char *tag;
	const char *string;
	size_t len;
};

#define T_(tag)		{ #tag, tag, sizeof (tag)-1 }
#define numof(a)	(sizeof (a) / sizeof ((a)[0]))

const struct sysdep_pair sysdep_pair_table[] = {
	/* this table must be sorted in the dictionary order. */
	T_(PRIX16),
	T_(PRIX32),
	T_(PRIX64),
	T_(PRIX8),
	T_(PRIXFAST16),
	T_(PRIXFAST32),
	T_(PRIXFAST64),
	T_(PRIXFAST8),
	T_(PRIXLEAST16),
	T_(PRIXLEAST32),
	T_(PRIXLEAST64),
	T_(PRIXLEAST8),
	T_(PRIXMAX),
	T_(PRIXPTR),
	T_(PRId16),
	T_(PRId32),
	T_(PRId64),
	T_(PRId8),
	T_(PRIdFAST16),
	T_(PRIdFAST32),
	T_(PRIdFAST64),
	T_(PRIdFAST8),
	T_(PRIdLEAST16),
	T_(PRIdLEAST32),
	T_(PRIdLEAST64),
	T_(PRIdLEAST8),
	T_(PRIdMAX),
	T_(PRIdPTR),
	T_(PRIi16),
	T_(PRIi32),
	T_(PRIi64),
	T_(PRIi8),
	T_(PRIiFAST16),
	T_(PRIiFAST32),
	T_(PRIiFAST64),
	T_(PRIiFAST8),
	T_(PRIiLEAST16),
	T_(PRIiLEAST32),
	T_(PRIiLEAST64),
	T_(PRIiLEAST8),
	T_(PRIiMAX),
	T_(PRIiPTR),
	T_(PRIo16),
	T_(PRIo32),
	T_(PRIo64),
	T_(PRIo8),
	T_(PRIoFAST16),
	T_(PRIoFAST32),
	T_(PRIoFAST64),
	T_(PRIoFAST8),
	T_(PRIoLEAST16),
	T_(PRIoLEAST32),
	T_(PRIoLEAST64),
	T_(PRIoLEAST8),
	T_(PRIoMAX),
	T_(PRIoPTR),
	T_(PRIu16),
	T_(PRIu32),
	T_(PRIu64),
	T_(PRIu8),
	T_(PRIuFAST16),
	T_(PRIuFAST32),
	T_(PRIuFAST64),
	T_(PRIuFAST8),
	T_(PRIuLEAST16),
	T_(PRIuLEAST32),
	T_(PRIuLEAST64),
	T_(PRIuLEAST8),
	T_(PRIuMAX),
	T_(PRIuPTR),
	T_(PRIx16),
	T_(PRIx32),
	T_(PRIx64),
	T_(PRIx8),
	T_(PRIxFAST16),
	T_(PRIxFAST32),
	T_(PRIxFAST64),
	T_(PRIxFAST8),
	T_(PRIxLEAST16),
	T_(PRIxLEAST32),
	T_(PRIxLEAST64),
	T_(PRIxLEAST8),
	T_(PRIxMAX),
	T_(PRIxPTR),
	T_(SCNd16),
	T_(SCNd32),
	T_(SCNd64),
	T_(SCNd8),
	T_(SCNdFAST16),
	T_(SCNdFAST32),
	T_(SCNdFAST64),
	T_(SCNdFAST8),
	T_(SCNdLEAST16),
	T_(SCNdLEAST32),
	T_(SCNdLEAST64),
	T_(SCNdLEAST8),
	T_(SCNdMAX),
	T_(SCNdPTR),
	T_(SCNi16),
	T_(SCNi32),
	T_(SCNi64),
	T_(SCNi8),
	T_(SCNiFAST16),
	T_(SCNiFAST32),
	T_(SCNiFAST64),
	T_(SCNiFAST8),
	T_(SCNiLEAST16),
	T_(SCNiLEAST32),
	T_(SCNiLEAST64),
	T_(SCNiLEAST8),
	T_(SCNiMAX),
	T_(SCNiPTR),
	T_(SCNo16),
	T_(SCNo32),
	T_(SCNo64),
	T_(SCNo8),
	T_(SCNoFAST16),
	T_(SCNoFAST32),
	T_(SCNoFAST64),
	T_(SCNoFAST8),
	T_(SCNoLEAST16),
	T_(SCNoLEAST32),
	T_(SCNoLEAST64),
	T_(SCNoLEAST8),
	T_(SCNoMAX),
	T_(SCNoPTR),
	T_(SCNu16),
	T_(SCNu32),
	T_(SCNu64),
	T_(SCNu8),
	T_(SCNuFAST16),
	T_(SCNuFAST32),
	T_(SCNuFAST64),
	T_(SCNuFAST8),
	T_(SCNuLEAST16),
	T_(SCNuLEAST32),
	T_(SCNuLEAST64),
	T_(SCNuLEAST8),
	T_(SCNuMAX),
	T_(SCNuPTR),
	T_(SCNx16),
	T_(SCNx32),
	T_(SCNx64),
	T_(SCNx8),
	T_(SCNxFAST16),
	T_(SCNxFAST32),
	T_(SCNxFAST64),
	T_(SCNxFAST8),
	T_(SCNxLEAST16),
	T_(SCNxLEAST32),
	T_(SCNxLEAST64),
	T_(SCNxLEAST8),
	T_(SCNxMAX),
	T_(SCNxPTR)
};

static int
cmp_sysdep_tag(const void *tag, const void *elem)
{
	const struct sysdep_pair *pair = elem;

	return strcmp(tag, pair->tag);
}

const char *
__intl_sysdep_get_string_by_tag(const char *tag, size_t *rlen)
{
	const struct sysdep_pair *found;

	found = bsearch(tag, sysdep_pair_table, numof(sysdep_pair_table),
			sizeof(sysdep_pair_table[0]), &cmp_sysdep_tag);

	if (found) {
		if (rlen)
			*rlen = found->len;
		return found->string;
	}

	if (rlen)
		*rlen = 0;
	return "";
}
