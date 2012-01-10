/*	$NetBSD: sparc64.c,v 1.18 2010/01/14 16:27:49 tsutsui Exp $	*/

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

/*
 * Copyright (c) 2002 Matthew R. Green
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#if !defined(__lint)
__RCSID("$NetBSD: sparc64.c,v 1.18 2010/01/14 16:27:49 tsutsui Exp $");
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

static int sparc64_clearboot(ib_params *);
static int sparc64_setboot(ib_params *);

struct ib_mach ib_mach_sparc64 =
	{ "sparc64", sparc64_setboot, sparc64_clearboot, no_editboot, 0};

static int
sparc64_clearboot(ib_params *params)
{
	char	bb[SPARC64_BOOT_BLOCK_MAX_SIZE];
	ssize_t	rv;

	assert(params != NULL);
	assert(params->fsfd != -1);
	assert(params->filesystem != NULL);

	if (params->flags & (IB_STAGE1START | IB_STAGE2START)) {
		warnx("`-b bno' and `-B bno' are not supported for %s",
		    params->machine->name);
		return (0);
	}

	/* first check that it _could_ exist here */
	rv = pread(params->fsfd, &bb, sizeof(bb), SPARC64_BOOT_BLOCK_OFFSET);
	if (rv == -1) {
		warn("Reading `%s'", params->filesystem);
		return (0);
	} else if (rv != sizeof(bb)) {
		warnx("Reading `%s': short read", params->filesystem);
		return (0);
	}

	/* now clear it out to nothing */
	memset(&bb, 0, sizeof(bb));

	if (params->flags & IB_VERBOSE)
		printf("%slearing boot block\n",
		    (params->flags & IB_NOWRITE) ? "Not c" : "C");
	if (params->flags & IB_NOWRITE)
		return (1);

	rv = pwrite(params->fsfd, &bb, sizeof(bb), SPARC64_BOOT_BLOCK_OFFSET);
	if (rv == -1) {
		warn("Writing `%s'", params->filesystem);
		return (0);
	} else if (rv != sizeof(bb)) {
		warnx("Writing `%s': short write", params->filesystem);
		return (0);
	}

	return (1);
}

static int
sparc64_setboot(ib_params *params)
{
	char		bb[SPARC64_BOOT_BLOCK_MAX_SIZE];
	int		retval;
	ssize_t		rv;

	assert(params != NULL);
	assert(params->fsfd != -1);
	assert(params->filesystem != NULL);
	assert(params->s1fd != -1);
	assert(params->stage1 != NULL);

	retval = 0;

	if (params->flags & (IB_STAGE1START | IB_STAGE2START)) {
		warnx("`-b bno' and `-B bno' are not supported for %s",
		    params->machine->name);
		goto done;
	}

	memset(&bb, 0, SPARC64_BOOT_BLOCK_MAX_SIZE);
	rv = read(params->s1fd, &bb, sizeof(bb));
	if (rv == -1) {
		warn("Reading `%s'", params->stage1);
		goto done;
	}

	if (params->flags & IB_VERBOSE) {
		printf("Bootstrap start sector: %u\n",
		    SPARC64_BOOT_BLOCK_OFFSET / SPARC64_BOOT_BLOCK_BLOCKSIZE);
		printf("Bootstrap byte count:   %u\n", (unsigned)rv);
		printf("%sriting bootstrap\n",
		    (params->flags & IB_NOWRITE) ? "Not w" : "W");
	}
	if (params->flags & IB_NOWRITE) {
		retval = 1;
		goto done;
	}

	rv = pwrite(params->fsfd, &bb, SPARC64_BOOT_BLOCK_MAX_SIZE,
	    SPARC64_BOOT_BLOCK_OFFSET);
	if (rv == -1) {
		warn("Writing `%s'", params->filesystem);
		goto done;
	} else if (rv != SPARC64_BOOT_BLOCK_MAX_SIZE) {
		warnx("Writing `%s': short write", params->filesystem);
		goto done;
	} else
		retval = 1;

 done:
	return (retval);
}
