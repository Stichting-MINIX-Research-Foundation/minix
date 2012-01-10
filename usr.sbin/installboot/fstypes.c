/*	$NetBSD: fstypes.c,v 1.13 2010/01/14 16:27:49 tsutsui Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Fredette and Luke Mewburn.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#if !defined(__lint)
__RCSID("$NetBSD: fstypes.c,v 1.13 2010/01/14 16:27:49 tsutsui Exp $");
#endif	/* !__lint */

#include <sys/types.h>

#include <assert.h>
#include <err.h>
#include <stdio.h>

#include "installboot.h"

struct ib_fs fstypes[] = {
#ifndef NO_STAGE2
	{ .name = "ffs",  .match = ffs_match,	.findstage2 = ffs_findstage2	},
	{ .name = "raid", .match = raid_match,	.findstage2 = ffs_findstage2	},
	{ .name = "raw",  .match = raw_match,	.findstage2 = raw_findstage2	},
#endif
	{ .name = NULL, }
};

#ifndef NO_STAGE2
int
hardcode_stage2(ib_params *params, uint32_t *maxblk, ib_block *blocks)
{
	struct stat	s2sb;
	uint32_t	nblk, i;

	assert(params != NULL);
	assert(params->stage2 != NULL);
	assert(maxblk != NULL);
	assert(blocks != NULL);
	assert((params->flags & IB_STAGE2START) != 0);
	assert(params->fstype != NULL);
	assert(params->fstype->blocksize != 0);

	if (stat(params->stage2, &s2sb) == -1) {
		warn("Examining `%s'", params->stage2);
		return (0);
	}
	if (!S_ISREG(s2sb.st_mode)) {
		warnx("`%s' must be a regular file", params->stage2);
		return (0);
	}

	nblk = s2sb.st_size / params->fstype->blocksize;
	if (s2sb.st_size % params->fstype->blocksize != 0)
		nblk++;
#if 0
	fprintf(stderr, "for %s got size %lld blksize %u blocks %u\n",
	    params->stage2, s2sb.st_size, params->fstype->blocksize, nblk);
#endif
	if (nblk > *maxblk) {
                warnx("Secondary bootstrap `%s' has too many blocks "
                    "(calculated %u, maximum %u)",
		    params->stage2, nblk, *maxblk);
                return (0);
        }

	for (i = 0; i < nblk; i++) {
		blocks[i].block = params->s2start +
		    i * (params->fstype->blocksize / params->sectorsize);
		blocks[i].blocksize = params->fstype->blocksize;
	}
	*maxblk = nblk;

	return (1);
}


int
raw_match(ib_params *params)
{

	assert(params != NULL);
	assert(params->fstype != NULL);

	params->fstype->blocksize = 8192;		// XXX: hardcode
	return (1);		/* can always write to a "raw" file system */
}

int
raw_findstage2(ib_params *params, uint32_t *maxblk, ib_block *blocks)
{

	assert(params != NULL);
	assert(params->stage2 != NULL);
	assert(maxblk != NULL);
	assert(blocks != NULL);

	if ((params->flags & IB_STAGE2START) == 0) {
		warnx("Need `-B bno' for raw file systems");
		return (0);
	}
	return (hardcode_stage2(params, maxblk, blocks));
}
#endif
