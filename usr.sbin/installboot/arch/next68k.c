/* $NetBSD: next68k.c,v 1.7 2010/01/07 13:26:00 tsutsui Exp $ */

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by David Laight and Christian Limpach.
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
__RCSID("$NetBSD: next68k.c,v 1.7 2010/01/07 13:26:00 tsutsui Exp $");
#endif /* !__lint */

#include <sys/param.h>

#include <assert.h>
#include <err.h>
#include <md5.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "installboot.h"

static uint16_t nextstep_checksum(const void *, const void *);
static int next68k_setboot(ib_params *);

struct ib_mach ib_mach_next68k =
	{ "next68k", next68k_setboot, no_clearboot, no_editboot, 0};

static uint16_t
nextstep_checksum(const void *vbuf, const void *vlimit)
{
	const uint16_t *buf = vbuf;
	const uint16_t *limit = vlimit;
	u_int sum = 0;

	while (buf < limit) {
		sum += be16toh(*buf++);
	}
	sum += (sum >> 16);
	return (sum & 0xffff);
}

static int
next68k_setboot(ib_params *params)
{
	int retval, labelupdated;
	uint8_t *bootbuf;
	size_t bootsize;
	ssize_t rv;
	uint32_t cd_secsize;
	int sec_netonb_mult;
	struct next68k_disklabel *next68klabel;
	uint16_t *checksum;
	uint32_t fp, b0, b1;

	assert(params != NULL);
	assert(params->fsfd != -1);
	assert(params->filesystem != NULL);
	assert(params->s1fd != -1);
	assert(params->stage1 != NULL);

	retval = 0;
	labelupdated = 0;
	bootbuf = NULL;

	next68klabel = malloc(NEXT68K_LABEL_SIZE);
	if (next68klabel == NULL) {
		warn("Allocating %lu bytes", (unsigned long)NEXT68K_LABEL_SIZE);
		goto done;
	}

	/*
	 * Read in the next68k disklabel
	 */
	rv = pread(params->fsfd, next68klabel, NEXT68K_LABEL_SIZE,
	    NEXT68K_LABEL_SECTOR * params->sectorsize + NEXT68K_LABEL_OFFSET);
	if (rv == -1) {
		warn("Reading `%s'", params->filesystem);
		goto done;
	}
	if (rv != NEXT68K_LABEL_SIZE) {
		warnx("Reading `%s': short read", params->filesystem);
		goto done;
	}
	if (be32toh(next68klabel->cd_version) == NEXT68K_LABEL_CD_V3) {
		checksum = &next68klabel->NEXT68K_LABEL_cd_v3_checksum;
	} else {
		checksum = &next68klabel->cd_checksum;
	}
	if (nextstep_checksum (next68klabel, checksum) !=
	    be16toh(*checksum)) {
		warn("Disklabel checksum invalid on `%s'",
		    params->filesystem);
		goto done;
	}

	cd_secsize = be32toh(next68klabel->cd_secsize);
	sec_netonb_mult = (cd_secsize / params->sectorsize);

	/*
	 * Allocate a buffer, with space to round up the input file
	 * to the next block size boundary, and with space for the boot
	 * block.
	 */
	bootsize = roundup(params->s1stat.st_size, cd_secsize);

	bootbuf = malloc(bootsize);
	if (bootbuf == NULL) {
		warn("Allocating %zu bytes", bootsize);
		goto done;
	}
	memset(bootbuf, 0, bootsize);

	/*
	 * Read the file into the buffer.
	 */
	rv = pread(params->s1fd, bootbuf, params->s1stat.st_size, 0);
	if (rv == -1) {
		warn("Reading `%s'", params->stage1);
		goto done;
	} else if (rv != params->s1stat.st_size) {
		warnx("Reading `%s': short read", params->stage1);
		goto done;
	}

	if (bootsize > be16toh(next68klabel->cd_front) * cd_secsize - 
	    NEXT68K_LABEL_SIZE) {
		warnx("Boot program is larger than front porch space");
		goto done;
	}

	fp = be16toh(next68klabel->cd_front);
	b0 = be32toh(next68klabel->cd_boot_blkno[0]);
	b1 = be32toh(next68klabel->cd_boot_blkno[1]);

	if (b0 > fp)
		b0 = fp;
	if (b1 > fp)
		b1 = fp;
	if (((bootsize / cd_secsize) > b1 - b0) ||
	    ((bootsize / cd_secsize) > fp - b1)) {
		if (2 * bootsize > (fp * cd_secsize - NEXT68K_LABEL_SIZE))
			/* can only fit one copy */
			b0 = b1 = NEXT68K_LABEL_SIZE / cd_secsize;
		else {
			if (2 * bootsize > (fp * cd_secsize - 
				NEXT68K_LABEL_DEFAULTBOOT0_1 *
				params->sectorsize))
				/* can fit two copies starting after label */
				b0 = NEXT68K_LABEL_SIZE / cd_secsize;
			else
				/* can fit two copies starting at default 1 */
				b0 = NEXT68K_LABEL_DEFAULTBOOT0_1 /
					sec_netonb_mult;
			/* try to fit 2nd copy at default 2 */
			b1 = NEXT68K_LABEL_DEFAULTBOOT0_2 / sec_netonb_mult;
			if (fp < b1)
				b1 = fp;
			if (bootsize / cd_secsize > (fp - b1))
				/* fit 2nd copy before front porch */
				b1 = fp - bootsize / cd_secsize;
		}
	}
	if (next68klabel->cd_boot_blkno[0] != (int32_t)htobe32(b0)) {
		next68klabel->cd_boot_blkno[0] = htobe32(b0);
		labelupdated = 1;
	}
	if (next68klabel->cd_boot_blkno[1] != (int32_t)htobe32(b1)) {
		next68klabel->cd_boot_blkno[1] = htobe32(b1);
		labelupdated = 1;
	}
	if (params->flags & IB_VERBOSE)
		printf("Boot programm locations%s: %d %d\n",
		    labelupdated ? " updated" : "", b0 * sec_netonb_mult,
		    b1 * sec_netonb_mult);

	if (params->flags & IB_NOWRITE) {
		retval = 1;
		goto done;
	}

	/*
	 * Write the updated next68k disklabel
	 */
	if (labelupdated) {
		if (params->flags & IB_VERBOSE)
			printf ("Writing updated label\n");
		*checksum = htobe16(nextstep_checksum (next68klabel,
					checksum));
		rv = pwrite(params->fsfd, next68klabel, NEXT68K_LABEL_SIZE,
		    NEXT68K_LABEL_SECTOR * params->sectorsize +
		    NEXT68K_LABEL_OFFSET);
		if (rv == -1) {
			warn("Writing `%s'", params->filesystem);
			goto done;
		}
		if (rv != NEXT68K_LABEL_SIZE) {
			warnx("Writing `%s': short write", params->filesystem);
			goto done;
		}
	}
	
	b0 *= sec_netonb_mult;
	b1 *= sec_netonb_mult;

	/*
	 * Write boot program to locations b0 and b1 (if different).
	 */
	for (;;) {
		if (params->flags & IB_VERBOSE)
			printf ("Writing boot program at %d\n", b0);
		rv = pwrite(params->fsfd, bootbuf, bootsize,
		    b0 * params->sectorsize);
		if (rv == -1) {
			warn("Writing `%s' at %d", params->filesystem, b0);
			goto done;
		}
		if ((size_t)rv != bootsize) {
			warnx("Writing `%s' at %d: short write", 
			    params->filesystem, b0);
			goto done;
		}
		if (b0 == b1)
			break;
		b0 = b1;
	}

	retval = 1;

 done:
	if (bootbuf)
		free(bootbuf);
	if (next68klabel)
		free(next68klabel);
	return retval;
}
