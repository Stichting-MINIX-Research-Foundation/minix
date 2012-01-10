/*	$NetBSD: x68k.c,v 1.4 2008/04/28 20:24:16 martin Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Luke Mewburn of Wasabi Systems.
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
__RCSID("$NetBSD: x68k.c,v 1.4 2008/04/28 20:24:16 martin Exp $");
#endif	/* !__lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <assert.h>
#include <err.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "installboot.h"

#define X68K_LABELOFFSET		64
#define X68K_LABELSIZE		404 /* reserve 16 partitions */

static int x68k_clearheader(ib_params *, struct bbinfo_params *, uint8_t *);

static int x68k_clearboot(ib_params *);
static int x68k_setboot(ib_params *);

struct ib_mach ib_mach_x68k =
	{ "x68k", x68k_setboot, x68k_clearboot, no_editboot,
		IB_STAGE1START | IB_STAGE2START };

static struct bbinfo_params bbparams = {
	X68K_BBINFO_MAGIC,
	X68K_BOOT_BLOCK_OFFSET,
	X68K_BOOT_BLOCK_BLOCKSIZE,
	X68K_BOOT_BLOCK_MAX_SIZE,
	X68K_LABELOFFSET + X68K_LABELSIZE, /* XXX */
	BBINFO_BIG_ENDIAN,
};

static int
x68k_clearboot(ib_params *params)
{

	assert(params != NULL);

	if (params->flags & IB_STAGE1START) {
		warnx("`-b bno' is not supported for %s",
			params->machine->name);
		return 0;
	}
	return shared_bbinfo_clearboot(params, &bbparams, x68k_clearheader);
}

static int
x68k_clearheader(ib_params *params, struct bbinfo_params *bb_params,
	uint8_t *bb)
{

	assert(params != NULL);
	assert(bb_params != NULL);
	assert(bb != NULL);

	memset(bb, 0, X68K_LABELOFFSET);
	return 1;
}

static int
x68k_setboot(ib_params *params)
{
	struct stat	bootstrapsb;
	char		bb[X68K_BOOT_BLOCK_MAX_SIZE];
	char		label[X68K_LABELSIZE];
	uint32_t	s1start;
	int		retval;
	ssize_t		rv;

	assert(params != NULL);
	assert(params->fsfd != -1);
	assert(params->filesystem != NULL);
	assert(params->s1fd != -1);
	assert(params->stage1 != NULL);

	retval = 0;

	if (params->flags & IB_STAGE1START)
		s1start = params->s1start;
	else
		s1start = X68K_BOOT_BLOCK_OFFSET /
		    X68K_BOOT_BLOCK_BLOCKSIZE;

	/* read disklabel on the target disk */
	rv = pread(params->fsfd, label, sizeof label,
		   s1start * X68K_BOOT_BLOCK_BLOCKSIZE + X68K_LABELOFFSET);
	if (rv == -1) {
		warn("Reading `%s'", params->filesystem);
		goto done;
	} else if (rv != sizeof label) {
		warnx("Reading `%s': short read", params->filesystem);
		goto done;
	}

	if (fstat(params->s1fd, &bootstrapsb) == -1) {
		warn("Examining `%s'", params->stage1);
		goto done;
	}
	if (!S_ISREG(bootstrapsb.st_mode)) {
		warnx("`%s' must be a regular file", params->stage1);
		goto done;
	}

	/* read boot loader */
	memset(&bb, 0, sizeof bb);
	rv = read(params->s1fd, &bb, sizeof bb);
	if (rv == -1) {
		warn("Reading `%s'", params->stage1);
		goto done;
	}
	/* then, overwrite disklabel */
	memcpy(&bb[X68K_LABELOFFSET], &label, sizeof label);

	if (params->flags & IB_VERBOSE) {
		printf("Bootstrap start sector: %#x\n", s1start);
		printf("Bootstrap byte count:   %#x\n", (unsigned)rv);
		printf("%sriting bootstrap\n",
		    (params->flags & IB_NOWRITE) ? "Not w" : "W");
	}
	if (params->flags & IB_NOWRITE) {
		retval = 1;
		goto done;
	}

	/* write boot loader and disklabel into the target disk */
	rv = pwrite(params->fsfd, &bb, X68K_BOOT_BLOCK_MAX_SIZE,
	    s1start * X68K_BOOT_BLOCK_BLOCKSIZE);
	if (rv == -1) {
		warn("Writing `%s'", params->filesystem);
		goto done;
	} else if (rv != X68K_BOOT_BLOCK_MAX_SIZE) {
		warnx("Writing `%s': short write", params->filesystem);
		goto done;
	} else
		retval = 1;

 done:
	return (retval);
}
