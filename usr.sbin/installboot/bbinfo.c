/*	$NetBSD: bbinfo.c,v 1.14 2009/04/05 11:55:39 lukem Exp $ */

/*-
 * Copyright (c) 1998, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Fredette, Paul Kranenburg, and Luke Mewburn.
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
__RCSID("$NetBSD: bbinfo.c,v 1.14 2009/04/05 11:55:39 lukem Exp $");
#endif	/* !__lint */

#include <sys/param.h>
#ifndef HAVE_NBTOOL_CONFIG_H
#include <sys/ioctl.h>
#include <sys/dkio.h>
#include <errno.h>
#endif

#include <assert.h>
#include <err.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "installboot.h"

int
shared_bbinfo_clearboot(ib_params *params, struct bbinfo_params *bbparams,
	int (*callback)(ib_params *, struct bbinfo_params *, uint8_t *))
{
	uint8_t	*bb;
	ssize_t	rv;
	int	retval;

	assert(params != NULL);
	assert(params->fsfd != -1);
	assert(params->filesystem != NULL);
	assert(bbparams != NULL);
	assert((strlen(bbparams->magic) + 1) == 32);

	retval = 0;
	if ((bb = malloc(bbparams->maxsize)) == NULL) {
		warn("Allocating %lu bytes for bbinfo",
		    (unsigned long) bbparams->maxsize);
		goto done;
	}

		/* First check that it _could_ exist here */
	rv = pread(params->fsfd, bb, bbparams->maxsize, bbparams->offset);
	if (rv == -1) {
		warn("Reading `%s'", params->filesystem);
		goto done;
	} else if ((uint32_t)rv != bbparams->maxsize) {
		warnx("Reading `%s': short read", params->filesystem);
		goto done;
	}

		/* Now clear out (past the header offset) */
	memset(bb + bbparams->headeroffset, 0,
	    bbparams->maxsize - bbparams->headeroffset);
	if (callback != NULL && ! (*callback)(params, bbparams, bb))
		goto done;

	if (params->flags & IB_VERBOSE)
		printf("%slearing boot block\n",
		    (params->flags & IB_NOWRITE) ? "Not c" : "C");
	if (params->flags & IB_NOWRITE) {
		retval = 1;
		goto done;
	}

	rv = pwrite(params->fsfd, bb, bbparams->maxsize, bbparams->offset);
#ifdef DIOCWLABEL
	if (rv == -1 && errno == EROFS) {
		/*
		 * The first sector might be protected by
		 * bounds_check_with_label(9)
		 */
		int enable;

		enable = 1;
		rv = ioctl(params->fsfd, DIOCWLABEL, &enable);
		if (rv != 0) {
			warn("Cannot enable writes to the label sector");
			goto done;
		}

		rv = pwrite(params->fsfd, bb, bbparams->maxsize,
		    bbparams->offset);

		/* Reset write-protect. */
		enable = 0;
		(void)ioctl(params->fsfd, DIOCWLABEL, &enable);
	}
#endif
	if (rv == -1) {
		warn("Writing `%s'", params->filesystem);
		goto done;
	} else if ((uint32_t)rv != bbparams->maxsize) {
		warnx("Writing `%s': short write", params->filesystem);
		goto done;
	} else
		retval = 1;

 done:
	if (bb != NULL)
		free(bb);
	return (retval);
}

int
shared_bbinfo_setboot(ib_params *params, struct bbinfo_params *bbparams,
	int (*callback)(ib_params *, struct bbinfo_params *, uint8_t *))
{
	uint8_t		*bb;
	int		retval;
	ssize_t		rv;
	size_t		bbi;
	struct shared_bbinfo	*bbinfop;	/* bbinfo in prototype image */
	uint32_t	maxblk, nblk, blk_i;
	ib_block	*blocks;

	assert(params != NULL);
	assert(params->fsfd != -1);
	assert(params->filesystem != NULL);
	assert(params->fstype != NULL);
	assert(params->s1fd != -1);
	assert(params->stage1 != NULL);
	assert(bbparams != NULL);
	assert((strlen(bbparams->magic) + 1) == 32);

	bbinfop = NULL;		/* XXXGCC -Wuninitialized [sparc64] */
	retval = 0;
	blocks = NULL;
	if ((bb = malloc(bbparams->maxsize)) == NULL) {
		warn("Allocating %lu bytes for bbinfo",
		    (unsigned long) bbparams->maxsize);
		goto done;
	}

	if (params->stage2 == NULL) {
		warnx("Name of secondary bootstrap not provided");
		goto done;
	}

	if (params->s1stat.st_size >
	    bbparams->maxsize - bbparams->headeroffset) {
		warnx("`%s' cannot be larger than %lu bytes",
		    params->stage1, (unsigned long)(bbparams->maxsize -
			bbparams->headeroffset));
		goto done;
	}

	memset(bb, 0, bbparams->maxsize);
	rv = read(params->s1fd, bb + bbparams->headeroffset,
	    bbparams->maxsize - bbparams->headeroffset);
	if (rv == -1) {
		warn("Reading `%s'", params->stage1);
		goto done;
	}

		/*
		 * Quick sanity check that the bootstrap given
		 * is *not* an ELF executable.
		 */
	if (memcmp(bb + bbparams->headeroffset + 1, "ELF", strlen("ELF"))
	    == 0) {
		warnx("`%s' is an ELF executable; need raw binary",
		    params->stage1);
		goto done;
	}

#define HOSTTOTARGET32(x) ((bbparams->endian == BBINFO_LITTLE_ENDIAN) \
			    ? (uint32_t)htole32((x)) : (uint32_t)htobe32((x)))
#define TARGET32TOHOST(x) ((bbparams->endian == BBINFO_LITTLE_ENDIAN) \
			    ? (uint32_t)le32toh((x)) : (uint32_t)be32toh((x)))

		/* Look for the bbinfo structure. */
	bbinfop = NULL;
	for (bbi = 0; bbi < bbparams->maxsize; bbi += sizeof(uint32_t)) {
		bbinfop = (void *) (bb + bbparams->headeroffset + bbi);
		if (memcmp(bbinfop->bbi_magic, bbparams->magic,
			    sizeof(bbinfop->bbi_magic)) == 0)
			break;
	}
	if (bbi >= bbparams->maxsize) {
		warnx("%s bbinfo structure not found in `%s'",
		    params->machine->name, params->stage1);
		goto done;
	}
	maxblk = TARGET32TOHOST(bbinfop->bbi_block_count);
	if (maxblk == 0 || maxblk > (bbparams->maxsize / sizeof(uint32_t))) {
		warnx("%s bbinfo structure in `%s' has preposterous size `%u'",
		    params->machine->name, params->stage1, maxblk);
		goto done;
	}

		/* Allocate space for our block list. */
	blocks = malloc(sizeof(*blocks) * maxblk);
	if (blocks == NULL) {
		warn("Allocating %lu bytes",
		    (unsigned long)sizeof(*blocks) * maxblk);
		goto done;
	}

	if (S_ISREG(params->fsstat.st_mode)) {
		if (fsync(params->fsfd) == -1)
			warn("Synchronising file system `%s'",
			    params->filesystem);
	} else {
		/* Ensure the secondary bootstrap is on disk. */
		sync();
	}

		/* Collect the blocks for the secondary bootstrap. */
	nblk = maxblk;
	if (! params->fstype->findstage2(params, &nblk, blocks))
		goto done;
	if (nblk == 0) {
		warnx("Secondary bootstrap `%s' is empty",
		    params->stage2);
		goto done;
	}

		/* Save those blocks in the primary bootstrap. */
	bbinfop->bbi_block_count = HOSTTOTARGET32(nblk);
	bbinfop->bbi_block_size = HOSTTOTARGET32(blocks[0].blocksize);
	for (blk_i = 0; blk_i < nblk; blk_i++) {
		bbinfop->bbi_block_table[blk_i] =
		    HOSTTOTARGET32(blocks[blk_i].block);
		if (blocks[blk_i].blocksize < blocks[0].blocksize &&
		    blk_i + 1 != nblk) {
			warnx("Secondary bootstrap `%s' blocks do not have "
			    "a uniform size", params->stage2);
			goto done;
		}
	}
	if (callback != NULL && ! (*callback)(params, bbparams, bb))
		goto done;

	if (params->flags & IB_VERBOSE) {
		printf("Bootstrap start sector: %u\n",
		    bbparams->offset / bbparams->blocksize);
		printf("Bootstrap byte count:   %u\n", (unsigned)rv);
		printf("Bootstrap block table:  "
			"%u entries of %u bytes available, %u used:",
		    maxblk, blocks[0].blocksize, nblk);
		for (blk_i = 0; blk_i < nblk; blk_i++)
			printf(" %llu",
			    (unsigned long long)blocks[blk_i].block);
		printf("\n%sriting bootstrap\n",
		    (params->flags & IB_NOWRITE) ? "Not w" : "W");
	}
	if (params->flags & IB_NOWRITE) {
		retval = 1;
		goto done;
	}

	rv = pwrite(params->fsfd, bb, bbparams->maxsize, bbparams->offset);
#ifdef DIOCWLABEL
	if (rv == -1 && errno == EROFS) {
		/*
		 * The first sector might be protected by
		 * bounds_check_with_label(9)
		 */
		int enable;

		enable = 1;
		rv = ioctl(params->fsfd, DIOCWLABEL, &enable);
		if (rv != 0) {
			warn("Cannot enable writes to the label sector");
			goto done;
		}

		rv = pwrite(params->fsfd, bb, bbparams->maxsize,
		    bbparams->offset);

		/* Reset write-protect. */
		enable = 0;
		(void)ioctl(params->fsfd, DIOCWLABEL, &enable);
	}
#endif
	if (rv == -1) {
		warn("Writing `%s'", params->filesystem);
		goto done;
	} else if ((uint32_t)rv != bbparams->maxsize) {
		warnx("Writing `%s': short write", params->filesystem);
		goto done;
	} else {
		retval = 1;
	}

 done:
	if (blocks != NULL)
		free(blocks);
	if (bb != NULL)
		free(bb);
	return (retval);
}
