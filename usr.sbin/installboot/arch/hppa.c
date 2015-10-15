/*	$NetBSD: hppa.c,v 1.1 2014/02/24 07:23:44 skrll Exp $	*/

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
__RCSID("$NetBSD: hppa.c,v 1.1 2014/02/24 07:23:44 skrll Exp $");
#endif	/* !__lint */

/* We need the target disklabel.h, not the hosts one..... */
#ifdef HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#include <nbinclude/sys/disklabel.h>
#else
#include <sys/disklabel.h>
#endif
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

#define HPPA_LABELOFFSET	512
#define HPPA_LABELSIZE		404 /* reserve 16 partitions */
#define	HPPA_BOOT_BLOCK_SIZE	8192

static int hppa_clearboot(ib_params *);
static int hppa_setboot(ib_params *);

struct ib_mach ib_mach_hppa =
	{ "hppa", hppa_setboot, hppa_clearboot, no_editboot, 0};

static int
hppa_clearboot(ib_params *params)
{
	char		bb[HPPA_BOOT_BLOCK_SIZE];
	int		retval, eol;
	ssize_t		rv;

	assert(params != NULL);
	assert(params->fsfd != -1);
	assert(params->filesystem != NULL);

	retval = 0;

	/* read disklabel on the target disk */
	rv = pread(params->fsfd, bb, sizeof bb, 0);
	if (rv == -1) {
		warn("Reading `%s'", params->filesystem);
		goto done;
	} else if (rv != sizeof bb) {
		warnx("Reading `%s': short read", params->filesystem);
		goto done;
	}

	/* clear header */
	memset(bb, 0, HPPA_LABELOFFSET);
	eol = HPPA_LABELOFFSET + HPPA_LABELSIZE;
	memset(&bb[eol], 0, sizeof bb - eol);

	if (params->flags & IB_VERBOSE) {
		printf("%slearing bootstrap\n",
		    (params->flags & IB_NOWRITE) ? "Not c" : "C");
	}
	if (params->flags & IB_NOWRITE) {
		retval = 1;
		goto done;
	}

	rv = pwrite(params->fsfd, bb, sizeof bb, 0);
	if (rv == -1) {
		warn("Writing `%s'", params->filesystem);
		goto done;
	} else if (rv != HPPA_BOOT_BLOCK_SIZE) {
		warnx("Writing `%s': short write", params->filesystem);
		goto done;
	} else
		retval = 1;

 done:
	return (retval);
}

static int
hppa_setboot(ib_params *params)
{
	struct stat	bootstrapsb;
	char		bb[HPPA_BOOT_BLOCK_SIZE];
	struct {
		char	l_off[HPPA_LABELOFFSET];
		struct disklabel l;
		char	l_pad[HPPA_BOOT_BLOCK_SIZE
			    - HPPA_LABELOFFSET - sizeof(struct disklabel)];
	} label;
	unsigned int	secsize, npart;
	int		retval;
	ssize_t		rv;

	assert(params != NULL);
	assert(params->fsfd != -1);
	assert(params->filesystem != NULL);
	assert(params->s1fd != -1);
	assert(params->stage1 != NULL);

	retval = 0;

	/* read disklabel on the target disk */
	rv = pread(params->fsfd, &label, HPPA_BOOT_BLOCK_SIZE, 0);
	if (rv == -1) {
		warn("Reading `%s'", params->filesystem);
		goto done;
	} else if (rv != HPPA_BOOT_BLOCK_SIZE) {
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

	/* check if valid disklabel exists */
	secsize = be32toh(label.l.d_secsize);
	npart = be16toh(label.l.d_npartitions);
	if (label.l.d_magic != htobe32(DISKMAGIC) ||
	    label.l.d_magic2 != htobe32(DISKMAGIC) ||
	    secsize == 0 || secsize & (secsize - 1) ||
	    npart > MAXMAXPARTITIONS) {
		warnx("No disklabel in `%s'", params->filesystem);

	/* then check if boot partition exists */
	} else if (npart < 1 || label.l.d_partitions[0].p_size == 0) {
		warnx("Partition `a' doesn't exist in %s", params->filesystem);

	/* check if the boot partition is below 2GB */
	} else if (be32toh(label.l.d_partitions[0].p_offset) +
	    be32toh(label.l.d_partitions[0].p_size) >
	    ((unsigned)2*1024*1024*1024) / secsize) {
		warnx("Partition `a' of `%s' exceeds 2GB boundary.",
		    params->filesystem);
		warnx("It won't boot since hppa PDC can handle only 2GB.");
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
	memcpy(&bb[HPPA_LABELOFFSET], &label.l, HPPA_LABELSIZE);

	if (params->flags & IB_VERBOSE) {
		printf("Bootstrap start sector: %#x\n", 0);
		printf("Bootstrap byte count:   %#zx\n", rv);
		printf("%sriting bootstrap\n",
		    (params->flags & IB_NOWRITE) ? "Not w" : "W");
	}
	if (params->flags & IB_NOWRITE) {
		retval = 1;
		goto done;
	}

	/* write boot loader and disklabel into the target disk */
	rv = pwrite(params->fsfd, &bb, HPPA_BOOT_BLOCK_SIZE, 0);
	if (rv == -1) {
		warn("Writing `%s'", params->filesystem);
		goto done;
	} else if (rv != HPPA_BOOT_BLOCK_SIZE) {
		warnx("Writing `%s': short write", params->filesystem);
		goto done;
	} else
		retval = 1;

 done:
	return (retval);
}
