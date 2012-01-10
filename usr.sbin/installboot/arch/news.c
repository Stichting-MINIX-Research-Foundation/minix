/*	$NetBSD: news.c,v 1.7 2008/04/28 20:24:16 martin Exp $ */

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Luke Mewburn and Izumi Tsutsui.
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
__RCSID("$NetBSD: news.c,v 1.7 2008/04/28 20:24:16 martin Exp $");
#endif	/* !__lint */

#include <sys/param.h>

#include <assert.h>
#include <err.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "installboot.h"

static int news_copydisklabel(ib_params *, struct bbinfo_params *, uint8_t *);

static int news68k_clearboot(ib_params *);
static int news68k_setboot(ib_params *);
static int newsmips_clearboot(ib_params *);
static int newsmips_setboot(ib_params *);

struct ib_mach ib_mach_news68k =
	{ "news68k", news68k_setboot, news68k_clearboot, no_editboot,
		IB_STAGE2START };

struct ib_mach ib_mach_newsmips =
	{ "newsmips", newsmips_setboot, newsmips_clearboot, no_editboot,
		IB_STAGE2START };

/*
 * news68k specific support
 */

static struct bbinfo_params news68k_bbparams = {
	NEWS68K_BBINFO_MAGIC,
	NEWS_BOOT_BLOCK_OFFSET,		/* write all 8K (including disklabel) */
	NEWS_BOOT_BLOCK_BLOCKSIZE,
	NEWS_BOOT_BLOCK_MAX_SIZE,
	0,
	BBINFO_BIG_ENDIAN,
};

static int
news68k_clearboot(ib_params *params)
{

	assert(params != NULL);

	return (shared_bbinfo_clearboot(params, &news68k_bbparams,
	    news_copydisklabel));
}

static int
news68k_setboot(ib_params *params)
{

	assert(params != NULL);

	return (shared_bbinfo_setboot(params, &news68k_bbparams,
	    news_copydisklabel));
}


/*
 * newsmips specific support
 */

static struct bbinfo_params newsmips_bbparams = {
	NEWSMIPS_BBINFO_MAGIC,
	NEWS_BOOT_BLOCK_OFFSET,		/* write all 8K (including disklabel) */
	NEWS_BOOT_BLOCK_BLOCKSIZE,
	NEWS_BOOT_BLOCK_MAX_SIZE,
	0,
	BBINFO_BIG_ENDIAN,
};

static int
newsmips_clearboot(ib_params *params)
{

	assert(params != NULL);

	return (shared_bbinfo_clearboot(params, &newsmips_bbparams,
	    news_copydisklabel));
}

static int
newsmips_setboot(ib_params *params)
{

	assert(params != NULL);

	return (shared_bbinfo_setboot(params, &newsmips_bbparams,
	    news_copydisklabel));
}


/*
 * news_copydisklabel --
 *	copy disklabel from existing location on disk into bootstrap,
 *	as the primary bootstrap contains the disklabel.
 */
static int
news_copydisklabel(ib_params *params, struct bbinfo_params *bbparams,
	uint8_t *bb)
{
	uint8_t	boot00[NEWS_BOOT_BLOCK_BLOCKSIZE];
	ssize_t	rv;

	assert(params != NULL);
	assert(params->fsfd != -1);
	assert(bbparams != NULL);
	assert(bb != NULL);

		/* Read label sector to copy disklabel from */
	memset(boot00, 0, sizeof(boot00));
	rv = pread(params->fsfd, boot00, sizeof(boot00), 0);
	if (rv == -1) {
		warn("Reading label sector from `%s'", params->filesystem);
		return (0);
	}
		/* Copy disklabel */
	memcpy(bb + NEWS_BOOT_BLOCK_LABELOFFSET,
	    boot00 + NEWS_BOOT_BLOCK_LABELOFFSET,
	    sizeof(boot00) - NEWS_BOOT_BLOCK_LABELOFFSET);

	return (1);
}
