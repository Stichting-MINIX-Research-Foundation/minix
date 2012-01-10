/*	$NetBSD: alpha.c,v 1.21 2011/08/14 17:50:17 christos Exp $	*/

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
__RCSID("$NetBSD: alpha.c,v 1.21 2011/08/14 17:50:17 christos Exp $");
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

#define	SUN_DKMAGIC	55998		/* XXX: from <dev/sun/disklabel.h> */

static void	resum(ib_params *, struct alpha_boot_block * const bb,
			uint16_t *bb16);
static void	sun_bootstrap(ib_params *, struct alpha_boot_block * const);
static void	check_sparc(const struct alpha_boot_block * const,
			    const char *);

static int alpha_clearboot(ib_params *);
static int alpha_setboot(ib_params *);

struct ib_mach ib_mach_alpha =
	{ "alpha", alpha_setboot, alpha_clearboot, no_editboot,
		IB_STAGE1START | IB_ALPHASUM | IB_APPEND | IB_SUNSUM };

static int
alpha_clearboot(ib_params *params)
{
	struct alpha_boot_block	bb;
	uint64_t		cksum;
	ssize_t			rv;

	assert(params != NULL);
	assert(params->fsfd != -1);
	assert(params->filesystem != NULL);
	assert(sizeof(struct alpha_boot_block) == ALPHA_BOOT_BLOCK_BLOCKSIZE);

	if (params->flags & (IB_STAGE1START | IB_APPEND)) {
		warnx("Can't use `-b bno' or `-o append' with `-c'");
		return (0);
	}

	rv = pread(params->fsfd, &bb, sizeof(bb), ALPHA_BOOT_BLOCK_OFFSET);
	if (rv == -1) {
		warn("Reading `%s'", params->filesystem);
		return (0);
	} else if (rv != sizeof(bb)) {
		warnx("Reading `%s': short read", params->filesystem);
		return (0);
	}
	ALPHA_BOOT_BLOCK_CKSUM(&bb, &cksum);
	if (cksum != bb.bb_cksum) {		// XXX check bb_cksum endian?
		warnx(
	    "Old boot block checksum invalid (was %#llx, calculated %#llx)",
		    (unsigned long long)le64toh(bb.bb_cksum),
		    (unsigned long long)le64toh(cksum));
		warnx("Boot block invalid");
		return (0);
	}

	if (params->flags & IB_VERBOSE) {
		printf("Old bootstrap start sector: %llu\n",
		    (unsigned long long)le64toh(bb.bb_secstart));
		printf("Old bootstrap size:         %llu\n",
		    (unsigned long long)le64toh(bb.bb_secsize));
		printf("Old bootstrap checksum:     %#llx\n",
		    (unsigned long long)le64toh(bb.bb_cksum));
	}

	bb.bb_secstart = bb.bb_secsize = bb.bb_flags = 0;

	ALPHA_BOOT_BLOCK_CKSUM(&bb, &bb.bb_cksum);
	if (params->flags & IB_SUNSUM)
		sun_bootstrap(params, &bb);

	printf("New bootstrap start sector: %llu\n",
	    (unsigned long long)le64toh(bb.bb_secstart));
	printf("New bootstrap size:         %llu\n",
	    (unsigned long long)le64toh(bb.bb_secsize));
	printf("New bootstrap checksum:     %#llx\n",
	    (unsigned long long)le64toh(bb.bb_cksum));

	if (params->flags & IB_VERBOSE)
		printf("%slearing boot block\n",
		    (params->flags & IB_NOWRITE) ? "Not c" : "C");
	if (params->flags & IB_NOWRITE)
		return (1);

	rv = pwrite(params->fsfd, &bb, sizeof(bb), ALPHA_BOOT_BLOCK_OFFSET);
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
alpha_setboot(ib_params *params)
{
	struct alpha_boot_block	bb;
	uint64_t		startblock;
	int			retval;
	char			*bootstrapbuf;
	size_t			bootstrapsize;
	ssize_t			rv;

	assert(params != NULL);
	assert(params->fsfd != -1);
	assert(params->filesystem != NULL);
	assert(params->s1fd != -1);
	assert(params->stage1 != NULL);
	assert(sizeof(struct alpha_boot_block) == ALPHA_BOOT_BLOCK_BLOCKSIZE);

	retval = 0;
	bootstrapbuf = NULL;

	/*
	 * Allocate a buffer, with space to round up the input file
	 * to the next block size boundary, and with space for the boot
	 * block.
	 */
	bootstrapsize = roundup(params->s1stat.st_size,
	    ALPHA_BOOT_BLOCK_BLOCKSIZE);

	bootstrapbuf = malloc(bootstrapsize);
	if (bootstrapbuf == NULL) {
		warn("Allocating %lu bytes", (unsigned long) bootstrapsize);
		goto done;
	}
	memset(bootstrapbuf, 0, bootstrapsize);

	/* read the file into the buffer */
	rv = pread(params->s1fd, bootstrapbuf, params->s1stat.st_size, 0);
	if (rv == -1) {
		warn("Reading `%s'", params->stage1);
		goto done;
	} else if (rv != params->s1stat.st_size) {
		warnx("Reading `%s': short read", params->stage1);
		goto done;
	}

	rv = pread(params->fsfd, &bb, sizeof(bb), ALPHA_BOOT_BLOCK_OFFSET);
	if (rv == -1) {
		warn("Reading `%s'", params->filesystem);
		goto done;
	} else if (rv != sizeof(bb)) {
		warnx("Reading `%s': short read", params->filesystem);
		goto done;
	}

	if (params->flags & IB_SUNSUM)
		check_sparc(&bb, "Initial");

		/* fill in the updated bootstrap fields */
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
		    ALPHA_BOOT_BLOCK_BLOCKSIZE);
	} else if (params->flags & IB_STAGE1START) {
		startblock = params->s1start;
	} else {
		startblock = ALPHA_BOOT_BLOCK_OFFSET /
		    ALPHA_BOOT_BLOCK_BLOCKSIZE + 1;
	}

	bb.bb_secsize =
	    htole64(howmany(params->s1stat.st_size,
		ALPHA_BOOT_BLOCK_BLOCKSIZE));
	bb.bb_secstart = htole64(startblock);
	bb.bb_flags = 0;

	ALPHA_BOOT_BLOCK_CKSUM(&bb, &bb.bb_cksum);
	if (params->flags & IB_SUNSUM)
		sun_bootstrap(params, &bb);

	if (params->flags & IB_VERBOSE) {
		printf("Bootstrap start sector:  %llu\n",
		    (unsigned long long)startblock);
		printf("Bootstrap sector count:  %llu\n",
		    (unsigned long long)le64toh(bb.bb_secsize));
		printf("New boot block checksum: %#llx\n",
		    (unsigned long long)le64toh(bb.bb_cksum));
		printf("%sriting bootstrap\n",
		    (params->flags & IB_NOWRITE) ? "Not w" : "W");
	}
	if (params->flags & IB_NOWRITE) {
		retval = 1;
		goto done;
	}
	rv = pwrite(params->fsfd, bootstrapbuf, bootstrapsize,
	     startblock * ALPHA_BOOT_BLOCK_BLOCKSIZE);
	if (rv == -1) {
		warn("Writing `%s'", params->filesystem);
		goto done;
	} else if ((size_t)rv != bootstrapsize) {
		warnx("Writing `%s': short write", params->filesystem);
		goto done;
	}

	if (params->flags & IB_VERBOSE)
		printf("Writing boot block\n");
	rv = pwrite(params->fsfd, &bb, sizeof(bb), ALPHA_BOOT_BLOCK_OFFSET);
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


/*
 * The Sun and alpha checksums overlay, and the Sun magic number also
 * overlays the alpha checksum. If you think you are smart: stop here
 * and do exercise one: figure out how to salt unimportant uint16_t
 * words in mid-sector so that the alpha and sparc checksums match,
 * and so the Sun magic number is embedded in the alpha checksum.
 *
 * The last uint64_t in the sector is the alpha arithmetic checksum.
 * The last uint16_t in the sector is the sun xor checksum.
 * The penultimate uint16_t in the sector is the sun magic number.
 *
 *	A:   511     510     509     508     507     506     505     504
 *	S:   510     511     508     509     506     507     504     505
 *	63     :       :       :     32:31     :       :       :       0
 *	|      :       :       :      \:|      :       :       :       |
 *	7654321076543210765432107654321076543210765432107654321076543210
 *	|--  sparc   --||--  sparc   --|
 *	|-- checksum --||--  magic   --|
 *	|----------------------- alpha checksum -----------------------|
 *			1011111011011010
 *			   b   e   d   a
 */

static void
resum(ib_params *params, struct alpha_boot_block * const bb, uint16_t *bb16)
{
	static uint64_t lastsum;

	if (bb16 != NULL)
		memcpy(bb, bb16, sizeof(*bb));
	ALPHA_BOOT_BLOCK_CKSUM(bb, &bb->bb_cksum);
	if (bb16 != NULL)
		memcpy(bb16, bb, sizeof(*bb));
	if ((params->flags & IB_VERBOSE) && lastsum != bb->bb_cksum)
		printf("alpha checksum now %016llx\n",
		    (unsigned long long)le64toh(bb->bb_cksum));
	lastsum = bb->bb_cksum;
}

static void
sun_bootstrap(ib_params *params, struct alpha_boot_block * const bb)
{
#	define BB_ADJUST_OFFSET 64
	static char our_int16s[] = "\2\3\6\7\12";
	uint16_t i, j, chkdelta, sunsum, bb16[256];

	/*
	 * Theory: the alpha checksum is adjusted so bits 47:32 add up
	 * to the Sun magic number. Then, another adjustment is computed
	 * so bits 63:48 add up to the Sun checksum, and applied in pieces
	 * so it changes the alpha checksum but not the Sun value.
	 *
  	 * Note: using memcpy(3) instead of a union as a strict c89/c9x
  	 * conformance experiment and to avoid a public interface delta.
	 */
	assert(sizeof(bb16) == sizeof(*bb));
	memcpy(bb16, bb, sizeof(bb16));
	for (i = 0; our_int16s[i]; ++i) {
		j = BB_ADJUST_OFFSET + our_int16s[i];
		if (bb16[j]) {
			warnx("Non-zero bits %04x in bytes %d..%d",
			    bb16[j], j * 2, j * 2 + 1);
			bb16[j] = 0;
			resum(params, bb, bb16);
		}
	}
	/*
	 * Make alpha checksum <47:32> come out to the sun magic.
	 */
	bb16[BB_ADJUST_OFFSET + 2] = htobe16(SUN_DKMAGIC) - bb16[254];
	resum(params, bb, bb16);
	sunsum = compute_sunsum(bb16);		/* might be the final value */
	if (params->flags & IB_VERBOSE)
		printf("target sun checksum is %04x\n", sunsum);
	/*
	 * Arrange to have alpha 63:48 add up to the sparc checksum.
	 */
	chkdelta = sunsum - bb16[255];
	bb16[BB_ADJUST_OFFSET + 3] = chkdelta >> 1;
	bb16[BB_ADJUST_OFFSET + 7] = chkdelta >> 1;
	/*
	 * By placing half the correction in two different uint64_t words at
	 * positions 63:48, the sparc sum will not change but the alpha sum
	 * will have the full correction, but only if the target adjustment
	 * was even. If it was odd, reverse propagate the carry one place.
	 */
	if (chkdelta & 1) {
		if (params->flags & IB_VERBOSE)
			printf("target adjustment %04x was odd, correcting\n",
			    chkdelta);
		assert(bb16[BB_ADJUST_OFFSET + 6] == 0);
		assert(bb16[BB_ADJUST_OFFSET + 012] == 0);
		bb16[BB_ADJUST_OFFSET + 6] += 0x8000;
		bb16[BB_ADJUST_OFFSET + 012] += 0x8000;
	}
	resum(params, bb, bb16);
	if (params->flags & IB_VERBOSE)
		printf("final harmonized checksum: %016llx\n",
		    (unsigned long long)le64toh(bb->bb_cksum));
	check_sparc(bb, "Final");
}

static void
check_sparc(const struct alpha_boot_block * const bb, const char *when)
{
	uint16_t bb16[256];
#define wmsg "%s sparc %s 0x%04x invalid, expected 0x%04x"

	memcpy(bb16, bb, sizeof(bb16));
	if (compute_sunsum(bb16) != bb16[255])
		warnx(wmsg, when, "checksum", bb16[255], compute_sunsum(bb16));
	if (bb16[254] != htobe16(SUN_DKMAGIC))
		warnx(wmsg, when, "magic number", bb16[254],
		    htobe16(SUN_DKMAGIC));
}
