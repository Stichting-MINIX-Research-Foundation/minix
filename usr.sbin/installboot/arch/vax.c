/*	$NetBSD: vax.c,v 1.19 2019/05/07 04:35:31 thorpej Exp $	*/

/*-
 * Copyright (c) 1999, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Simon Burge.
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
 * Copyright (c) 1999 Christopher G. Demetriou.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#if !defined(__lint)
__RCSID("$NetBSD: vax.c,v 1.19 2019/05/07 04:35:31 thorpej Exp $");
#endif	/* !__lint */

#include <sys/param.h>
#ifdef HAVE_NBTOOL_CONFIG_H
#include <nbinclude/vax/disklabel.h>
#else
#include <sys/disklabel.h>
#endif

#include <assert.h>
#include <err.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "installboot.h"

#define	VAX_LABELOFFSET		64

#ifndef __CTASSERT
#define	__CTASSERT(X)
#endif

static int	load_bootstrap(ib_params *, char **,
		    uint32_t *, uint32_t *, size_t *);

static int vax_clearboot(ib_params *);
static int vax_setboot(ib_params *);

struct ib_mach ib_mach_vax = {
	.name		=	"vax",
	.setboot	=	vax_setboot,
	.clearboot	=	vax_clearboot,
	.editboot	=	no_editboot,
	.valid_flags	=	IB_STAGE1START | IB_APPEND | IB_SUNSUM,
};

static int
vax_clearboot(ib_params *params)
{
	struct vax_boot_block	bb;
	ssize_t			rv;

	assert(params != NULL);
	assert(params->fsfd != -1);
	assert(params->filesystem != NULL);
	__CTASSERT(sizeof(bb)==VAX_BOOT_BLOCK_BLOCKSIZE);

	rv = pread(params->fsfd, &bb, sizeof(bb), VAX_BOOT_BLOCK_OFFSET);
	if (rv == -1) {
		warn("Reading `%s'", params->filesystem);
		return (0);
	} else if (rv != sizeof(bb)) {
		warnx("Reading `%s': short read", params->filesystem);
		return (0);
	}

	if (bb.bb_id_offset*2 >= VAX_BOOT_BLOCK_BLOCKSIZE
	    || bb.bb_magic1 != VAX_BOOT_MAGIC1) {
		warnx(
		    "Old boot block magic number invalid; boot block invalid");
		return (0);
	}

	bb.bb_id_offset = 1;
	bb.bb_mbone = 0;
	bb.bb_lbn_hi = 0;
	bb.bb_lbn_low = 0;

	if (params->flags & IB_SUNSUM) {
		uint16_t	sum;

		sum = compute_sunsum((uint16_t *)&bb);
		if (! set_sunsum(params, (uint16_t *)&bb, sum))
			return (0);
	}

	if (params->flags & IB_VERBOSE)
		printf("%slearing boot block\n",
		    (params->flags & IB_NOWRITE) ? "Not c" : "C");
	if (params->flags & IB_NOWRITE)
		return (1);

	rv = pwrite(params->fsfd, &bb, sizeof(bb), VAX_BOOT_BLOCK_OFFSET);
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
vax_setboot(ib_params *params)
{
	struct stat		bootstrapsb;
	struct vax_boot_block	*bb;
	uint32_t		startblock;
	int			retval;
	char			*bootstrapbuf, oldbb[VAX_BOOT_BLOCK_BLOCKSIZE];
	size_t			bootstrapsize;
	uint32_t		bootstrapload, bootstrapexec;
	ssize_t			rv;

	assert(params != NULL);
	assert(params->fsfd != -1);
	assert(params->filesystem != NULL);
	assert(params->s1fd != -1);
	assert(params->stage1 != NULL);

	/* see sys/arch/vax/boot/xxboot/start.S for explanation */
	__CTASSERT(offsetof(struct vax_boot_block,bb_magic1) == 0x19e);
	__CTASSERT(sizeof(struct vax_boot_block) == VAX_BOOT_BLOCK_BLOCKSIZE);

	startblock = 0;
	retval = 0;
	bootstrapbuf = NULL;

	if (fstat(params->s1fd, &bootstrapsb) == -1) {
		warn("Examining `%s'", params->stage1);
		goto done;
	}
	if (!S_ISREG(bootstrapsb.st_mode)) {
		warnx("`%s' must be a regular file", params->stage1);
		goto done;
	}
	if (! load_bootstrap(params, &bootstrapbuf, &bootstrapload,
	    &bootstrapexec, &bootstrapsize))
		goto done;

	/* read old boot block */
	rv = pread(params->fsfd, oldbb, sizeof(oldbb), VAX_BOOT_BLOCK_OFFSET);
	if (rv == -1) {
		warn("Reading `%s'", params->filesystem);
		goto done;
	} else if (rv != sizeof(oldbb)) {
		warnx("Reading `%s': short read", params->filesystem);
		goto done;
	}

	/*
	 * Copy disklabel from old boot block to new.
	 * Assume everything between VAX_LABELOFFSET and the start of
	 * the param block is scratch area and can be copied over.
	 */
	memcpy(bootstrapbuf + VAX_LABELOFFSET,
	    oldbb + VAX_LABELOFFSET,
	    offsetof(struct vax_boot_block,bb_magic1) - VAX_LABELOFFSET);

	/* point to bootblock at begining of bootstrap */
	bb = (struct vax_boot_block*)bootstrapbuf;

	/* fill in the updated boot block fields */
	if (params->flags & IB_APPEND) {
		struct stat	filesyssb;

		if (fstat(params->fsfd, &filesyssb) == -1) {
			warn("Examining `%s'", params->filesystem);
			goto done;
		}
		if (!S_ISREG(filesyssb.st_mode)) {
			warnx(
		    "`%s' must be a regular file to append a bootstrap",
			    params->filesystem);
			goto done;
		}
		startblock = howmany(filesyssb.st_size,
		    VAX_BOOT_BLOCK_BLOCKSIZE);
		bb->bb_lbn_hi = htole16((uint16_t) (startblock >> 16));
		bb->bb_lbn_low = htole16((uint16_t) (startblock >>  0));
	}

	if (params->flags & IB_SUNSUM) {
		uint16_t	sum;

		sum = compute_sunsum((uint16_t *)bb);
		if (! set_sunsum(params, (uint16_t *)bb, sum))
			goto done;
	}

	if (params->flags & IB_VERBOSE) {
		printf("Bootstrap start sector: %u\n", startblock);
		printf("Bootstrap sector count: %u\n", le32toh(bb->bb_size));
		printf("%sriting bootstrap\n",
		    (params->flags & IB_NOWRITE) ? "Not w" : "W");
	}
	if (params->flags & IB_NOWRITE) {
		retval = 1;
		goto done;
	}
	rv = pwrite(params->fsfd, bootstrapbuf, bootstrapsize, 0);
	if (rv == -1) {
		warn("Writing `%s'", params->filesystem);
		goto done;
	} else if ((size_t)rv != bootstrapsize) {
		warnx("Writing `%s': short write", params->filesystem);
		goto done;
	}
	retval = 1;

 done:
	if (bootstrapbuf)
		free(bootstrapbuf);
	return (retval);
}

static int
load_bootstrap(ib_params *params, char **data,
	uint32_t *loadaddr, uint32_t *execaddr, size_t *len)
{
	ssize_t	cc;
	size_t	buflen;

	buflen = 512 * (VAX_BOOT_SIZE + 1);
	*data = malloc(buflen);
	if (*data == NULL) {
		warn("Allocating %lu bytes", (unsigned long) buflen);
		return (0);
	}

	cc = pread(params->s1fd, *data, buflen, 0);
	if (cc <= 0) {
		warn("Reading `%s'", params->stage1);
		return (0);
	}
	if (cc > 512 * VAX_BOOT_SIZE) {
		warnx("`%s': too large", params->stage1);
		return (0);
	}

	*len = roundup(cc, VAX_BOOT_BLOCK_BLOCKSIZE);
	*loadaddr = VAX_BOOT_LOAD;
	*execaddr = VAX_BOOT_ENTRY;
	return (1);
}
