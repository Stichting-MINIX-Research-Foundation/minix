/*	$NetBSD: pmax.c,v 1.14 2009/04/05 11:55:39 lukem Exp $	*/

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
 * Copyright (c) 1999 Ross Harvey.  All rights reserved.
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
 *      This product includes software developed by Ross Harvey
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
__RCSID("$NetBSD: pmax.c,v 1.14 2009/04/05 11:55:39 lukem Exp $");
#endif	/* !__lint */

#include <sys/param.h>

#include <assert.h>
#include <err.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/exec_elf.h>

#include "installboot.h"

static int	load_bootstrap(ib_params *, char **,
				uint32_t *, uint32_t *, size_t *);

static int pmax_clearboot(ib_params *);
static int pmax_setboot(ib_params *);

struct ib_mach ib_mach_pmax =
	{ "pmax", pmax_setboot, pmax_clearboot, no_editboot,
		IB_STAGE1START | IB_APPEND | IB_SUNSUM };


static int
pmax_clearboot(ib_params *params)
{
	struct pmax_boot_block	bb;
	ssize_t			rv;

	assert(params != NULL);
	assert(params->fsfd != -1);
	assert(params->filesystem != NULL);
	assert(sizeof(struct pmax_boot_block) == PMAX_BOOT_BLOCK_BLOCKSIZE);

	rv = pread(params->fsfd, &bb, sizeof(bb), PMAX_BOOT_BLOCK_OFFSET);
	if (rv == -1) {
		warn("Reading `%s'", params->filesystem);
		return (0);
	} else if (rv != sizeof(bb)) {
		warnx("Reading `%s': short read", params->filesystem);
		return (0);
	}

	if (le32toh(bb.magic) != PMAX_BOOT_MAGIC) {
		warnx(
		    "Old boot block magic number invalid; boot block invalid");
		return (0);
	}

	bb.map[0].num_blocks = bb.map[0].start_block = bb.mode = 0;
	bb.magic = htole32(PMAX_BOOT_MAGIC);

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

	rv = pwrite(params->fsfd, &bb, sizeof(bb), PMAX_BOOT_BLOCK_OFFSET);
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
pmax_setboot(ib_params *params)
{
	struct pmax_boot_block	bb;
	uint32_t		startblock;
	int			retval;
	char			*bootstrapbuf;
	size_t			bootstrapsize;
	uint32_t		bootstrapload, bootstrapexec;
	ssize_t			rv;

	assert(params != NULL);
	assert(params->fsfd != -1);
	assert(params->filesystem != NULL);
	assert(params->s1fd != -1);
	assert(params->stage1 != NULL);
	assert(sizeof(struct pmax_boot_block) == PMAX_BOOT_BLOCK_BLOCKSIZE);

	retval = 0;
	bootstrapbuf = NULL;

	if (! load_bootstrap(params, &bootstrapbuf, &bootstrapload,
	    &bootstrapexec, &bootstrapsize))
		goto done;

	rv = pread(params->fsfd, &bb, sizeof(bb), PMAX_BOOT_BLOCK_OFFSET);
	if (rv == -1) {
		warn("Reading `%s'", params->filesystem);
		goto done;
	} else if (rv != sizeof(bb)) {
		warnx("Reading `%s': short read", params->filesystem);
		goto done;
	}

		/* fill in the updated boot block fields */
	if (params->flags & IB_APPEND) {
		if (! S_ISREG(params->fsstat.st_mode)) {
			warnx(
		    "`%s' must be a regular file to append a bootstrap",
			    params->filesystem);
			goto done;
		}
		startblock = howmany(params->fsstat.st_size,
		    PMAX_BOOT_BLOCK_BLOCKSIZE);
	} else if (params->flags & IB_STAGE1START) {
		startblock = params->s1start;
	} else {
		startblock = PMAX_BOOT_BLOCK_OFFSET / PMAX_BOOT_BLOCK_BLOCKSIZE
		    + 1;
	}

	bb.map[0].start_block = htole32(startblock);
	bb.map[0].num_blocks =
	    htole32(howmany(bootstrapsize, PMAX_BOOT_BLOCK_BLOCKSIZE));
	bb.magic = htole32(PMAX_BOOT_MAGIC);
	bb.load_addr = htole32(bootstrapload);
	bb.exec_addr = htole32(bootstrapexec);
	bb.mode = htole32(PMAX_BOOTMODE_CONTIGUOUS);

	if (params->flags & IB_SUNSUM) {
		uint16_t	sum;

		sum = compute_sunsum((uint16_t *)&bb);
		if (! set_sunsum(params, (uint16_t *)&bb, sum))
			goto done;
	}

	if (params->flags & IB_VERBOSE) {
		printf("Bootstrap start sector: %u\n",
		    le32toh(bb.map[0].start_block));
		printf("Bootstrap sector count: %u\n",
		    le32toh(bb.map[0].num_blocks));
		printf("Bootstrap load address: %#x\n",
		    le32toh(bb.load_addr));
		printf("Bootstrap exec address: %#x\n",
		    le32toh(bb.exec_addr));
		printf("%sriting bootstrap\n",
		    (params->flags & IB_NOWRITE) ? "Not w" : "W");
	}
	if (params->flags & IB_NOWRITE) {
		retval = 1;
		goto done;
	}
	rv = pwrite(params->fsfd, bootstrapbuf, bootstrapsize,
	     startblock * PMAX_BOOT_BLOCK_BLOCKSIZE);
	if (rv == -1) {
		warn("Writing `%s'", params->filesystem);
		goto done;
	} else if ((size_t)rv != bootstrapsize) {
		warnx("Writing `%s': short write", params->filesystem);
		goto done;
	}

	if (params->flags & IB_VERBOSE)
		printf("Writing boot block\n");
	rv = pwrite(params->fsfd, &bb, sizeof(bb), PMAX_BOOT_BLOCK_OFFSET);
	if (rv == -1) {
		warn("Writing `%s'", params->filesystem);
		goto done;
	} else if (rv != sizeof(bb)) {
		warnx("Writing `%s': short write", params->filesystem);
		goto done;
	} else {
		retval = 1;
	}

 done:
	if (bootstrapbuf)
		free(bootstrapbuf);
	return (retval);
}


#define MAX_SEGMENTS	10	/* We can load up to 10 segments */

struct seglist {
	Elf32_Addr	addr;
	Elf32_Off	f_offset;
	Elf32_Word	f_size;
};

static int
load_bootstrap(ib_params *params, char **data,
	uint32_t *loadaddr, uint32_t *execaddr, size_t *len)
{
	int		i, nsegs;
	Elf32_Addr	lowaddr, highaddr;
	Elf32_Ehdr	ehdr;
	Elf32_Phdr	phdr;
	struct seglist	seglist[MAX_SEGMENTS];

	if ((pread(params->s1fd, &ehdr, sizeof(ehdr), 0)) != sizeof(ehdr)) {
		warn("Reading `%s'", params->stage1);
		return (0);
	}
	if ((memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0) ||
	    (ehdr.e_ident[EI_CLASS] != ELFCLASS32)) {
		warnx("No ELF header in `%s'", params->stage1);
		return (0);
	}

	nsegs = highaddr = 0;
	lowaddr = (uint32_t) ULONG_MAX;

	for (i = 0; i < le16toh(ehdr.e_phnum); i++) {
		if (pread(params->s1fd, &phdr, sizeof(phdr),
		    (off_t) le32toh(ehdr.e_phoff) + i * sizeof(phdr))
		    != sizeof(phdr)) {
			warn("Reading `%s'", params->stage1);
			return (0);
		}
		if (le32toh(phdr.p_type) != PT_LOAD)
			continue;

		seglist[nsegs].addr = le32toh(phdr.p_paddr);
		seglist[nsegs].f_offset = le32toh(phdr.p_offset);
		seglist[nsegs].f_size = le32toh(phdr.p_filesz);
		nsegs++;

		if (le32toh(phdr.p_paddr) < lowaddr)
			lowaddr = le32toh(phdr.p_paddr);
		if (le32toh(phdr.p_paddr) + le32toh(phdr.p_filesz) > highaddr)
			highaddr = le32toh(phdr.p_paddr) +
			    le32toh(phdr.p_filesz);
	}

	*loadaddr = lowaddr;
	*execaddr = le32toh(ehdr.e_entry);
	*len = roundup(highaddr - lowaddr, PMAX_BOOT_BLOCK_BLOCKSIZE);
	if ((*data = malloc(*len)) == NULL) {
		warn("Allocating %lu bytes", (unsigned long) *len);
		return (0);
	}

	/* Now load the bootstrap into memory */
	for (i = 0; i < nsegs; i++) {
		if ((Elf32_Word)pread(params->s1fd,
		    *data + seglist[i].addr - lowaddr,
		    seglist[i].f_size, (off_t)seglist[i].f_offset)
		    != seglist[i].f_size) {
			warn("Reading `%s'", params->stage1);
			return (0);
		}
	}
	return (1);
}
