/*	$NetBSD: bfs_sysvbfs.c,v 1.11 2015/03/28 19:24:05 maxv Exp $	*/

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by UCHIYAMA Yasushi.
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

#include <sys/cdefs.h>

__KERNEL_RCSID(0, "$NetBSD: bfs_sysvbfs.c,v 1.11 2015/03/28 19:24:05 maxv Exp $");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/kauth.h>
#include <fs/sysvbfs/bfs.h>

struct bc_io_ops {
	struct sector_io_ops io;
	struct vnode *vp;
	kauth_cred_t cred;
};

#define	STATIC

STATIC bool bc_read_n(void *, uint8_t *, daddr_t, int);
STATIC bool bc_read(void *, uint8_t *, daddr_t);
STATIC bool bc_write_n(void *, uint8_t *, daddr_t, int);
STATIC bool bc_write(void *, uint8_t *, daddr_t);

int
sysvbfs_bfs_init(struct bfs **bfsp, struct vnode *vp)
{
	struct bc_io_ops *bio;

	if ((bio = malloc(sizeof(*bio), M_TEMP, M_WAITOK | M_ZERO)) == NULL) {
		printf("can't allocate I/O ops.\n");
		return ENOMEM;
	}

	bio->io.read = bc_read;
	bio->io.read_n = bc_read_n;
	bio->io.write = bc_write;
	bio->io.write_n = bc_write_n;
	bio->vp = vp;
	bio->cred = NOCRED;	/* sysvbfs layer check cred. */

	return bfs_init2(bfsp, 0, (struct sector_io_ops *)bio, false);
}

void
sysvbfs_bfs_fini(struct bfs *bfs)
{

	free(bfs->io, M_TEMP);
	bfs_fini(bfs);
}

STATIC bool
bc_read_n(void *self, uint8_t *buf, daddr_t block, int count)
{
	int i;

	for (i = 0; i < count; i++) {
		if (!bc_read(self, buf, block))
			return false;
		buf += DEV_BSIZE;
		block++;
	}

	return true;
}

STATIC bool
bc_read(void *self, uint8_t *buf, daddr_t block)
{
	struct bc_io_ops *bio = self;
	struct buf *bp = NULL;

	if (bread(bio->vp, block, DEV_BSIZE, 0, &bp) != 0)
		goto error_exit;
	memcpy(buf, bp->b_data, DEV_BSIZE);
	brelse(bp, 0);

	return true;
 error_exit:
	printf("%s: block %lld read failed.\n", __func__, 
	    (long long int)block);

	if (bp != NULL)
		brelse(bp, 0);
	return false;
}

STATIC bool
bc_write_n(void *self, uint8_t *buf, daddr_t block, int count)
{
	int i;

	for (i = 0; i < count; i++) {
		if (!bc_write(self, buf, block))
			return false;
		buf += DEV_BSIZE;
		block++;
	}

	return true;
}

STATIC bool
bc_write(void *self, uint8_t *buf, daddr_t block)
{
	struct bc_io_ops *bio = self;
	struct buf *bp;

#if 0
	printf("%s: block=%lld\n", __func__, block);
#endif
	if ((bp = getblk(bio->vp, block, DEV_BSIZE, 0, 0)) == 0) {
		printf("getblk failed.\n");
		return false;
	}
	memcpy(bp->b_data, buf, DEV_BSIZE);

	if (bwrite(bp) != 0) {
		printf("bwrite failed.\n");
		return false;
	}

	return true;
}
