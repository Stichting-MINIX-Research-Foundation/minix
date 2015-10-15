/*	$NetBSD: v7fs_io_kern.c,v 1.3 2015/03/28 19:24:05 maxv Exp $	*/

/*-
 * Copyright (c) 2004, 2011 The NetBSD Foundation, Inc.
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
__KERNEL_RCSID(0, "$NetBSD: v7fs_io_kern.c,v 1.3 2015/03/28 19:24:05 maxv Exp $");
#if defined _KERNEL_OPT
#include "opt_v7fs.h"
#endif
#include <sys/cdefs.h>

__KERNEL_RCSID(0, "$NetBSD: v7fs_io_kern.c,v 1.3 2015/03/28 19:24:05 maxv Exp $");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/kmem.h>
#include <sys/kauth.h>
#include <sys/mutex.h>

#include <fs/v7fs/v7fs.h>
#include "v7fs_endian.h"
#include "v7fs_impl.h"

#ifdef V7FS_IO_DEBUG
#define	DPRINTF(fmt, args...)	printf("%s: " fmt, __func__, ##args)
#else
#define	DPRINTF(fmt, args...)	((void)0)
#endif

struct local_io {
	struct vnode *vp;
	kauth_cred_t cred;
};

static bool v7fs_os_read_n(void *, uint8_t *, daddr_t, int);
static bool v7fs_os_read(void *, uint8_t *, daddr_t);
static bool v7fs_os_write_n(void *, uint8_t *, daddr_t, int);
static bool v7fs_os_write(void *, uint8_t *, daddr_t);
static void v7fs_os_lock(void *);
static void v7fs_os_unlock(void *);
static bool lock_init(struct lock_ops *);

int
v7fs_io_init(struct v7fs_self **fs,
    const struct v7fs_mount_device *mount_device, size_t block_size)
{
	struct vnode *vp = mount_device->device.vnode;
	struct v7fs_self *p;
	struct local_io *local;
	int error = 0;

	if ((p = kmem_zalloc(sizeof(*p), KM_SLEEP)) == NULL)
		return ENOMEM;

	p->scratch_free = -1;
	p->scratch_remain = V7FS_SELF_NSCRATCH;

	/* Endian */
	p->endian = mount_device->endian;
#ifdef V7FS_EI
	v7fs_endian_init(p);
#endif
	/* IO */
	if ((local = kmem_zalloc(sizeof(*local), KM_SLEEP)) == NULL) {
		error = ENOMEM;
		goto errexit;
	}
	p->io.read = v7fs_os_read;
	p->io.read_n = v7fs_os_read_n;
	p->io.write = v7fs_os_write;
	p->io.write_n = v7fs_os_write_n;
	p->scratch_free = -1; /* free all scratch buffer */

	p->io.cookie = local;
	local->vp = vp;
	local->cred = NOCRED;	/* upper layer check cred. */

	/*LOCK */
	error = ENOMEM;
	if (!lock_init(&p->sb_lock))
		goto errexit;
	if (!lock_init(&p->ilist_lock))
		goto errexit;
	if (!lock_init(&p->mem_lock))
		goto errexit;
	error = 0;

	*fs = p;
	return 0;

errexit:
	v7fs_io_fini(p);
	return error;
}

static bool
lock_init(struct lock_ops *ops)
{
	if ((ops->cookie = kmem_zalloc(sizeof(kmutex_t), KM_SLEEP)) == NULL) {
		return false;
	}
	mutex_init(ops->cookie, MUTEX_DEFAULT, IPL_NONE);
	ops->lock = v7fs_os_lock;
	ops->unlock = v7fs_os_unlock;
	return true;
}

void
v7fs_io_fini(struct v7fs_self *fs)
{
	if (fs->io.cookie) {
		kmem_free(fs->io.cookie, sizeof(struct local_io));
	}
	if (fs->sb_lock.cookie) {
		mutex_destroy(fs->sb_lock.cookie);
		kmem_free(fs->sb_lock.cookie, sizeof(kmutex_t));
	}
	if (fs->ilist_lock.cookie) {
		mutex_destroy(fs->ilist_lock.cookie);
		kmem_free(fs->ilist_lock.cookie, sizeof(kmutex_t));
	}
	if (fs->mem_lock.cookie) {
		mutex_destroy(fs->mem_lock.cookie);
		kmem_free(fs->mem_lock.cookie, sizeof(kmutex_t));
	}
	kmem_free(fs, sizeof(*fs));
}

static bool
v7fs_os_read_n(void *self, uint8_t *buf, daddr_t block, int count)
{
	int i;

	for (i = 0; i < count; i++) {
		if (!v7fs_os_read(self, buf, block))
			return false;
		buf += DEV_BSIZE;
		block++;
	}

	return true;
}

static bool
v7fs_os_read(void *self, uint8_t *buf, daddr_t block)
{
	struct local_io *bio = (struct local_io *)self;
	struct buf *bp = NULL;

	if (bread(bio->vp, block, DEV_BSIZE, 0, &bp) != 0)
		goto error_exit;
	memcpy(buf, bp->b_data, DEV_BSIZE);
	brelse(bp, 0);

	return true;
error_exit:
	DPRINTF("block %ld read failed.\n", (long)block);

	if (bp != NULL)
		brelse(bp, 0);
	return false;
}

static bool
v7fs_os_write_n(void *self, uint8_t *buf, daddr_t block, int count)
{
	int i;

	for (i = 0; i < count; i++) {
		if (!v7fs_os_write(self, buf, block))
			return false;
		buf += DEV_BSIZE;
		block++;
	}

	return true;
}

static bool
v7fs_os_write(void *self, uint8_t *buf, daddr_t block)
{
	struct local_io *bio = (struct local_io *)self;
	struct buf *bp;

	if ((bp = getblk(bio->vp, block, DEV_BSIZE, 0, 0)) == 0) {
		DPRINTF("getblk failed. block=%ld\n", (long)block);
		return false;
	}

	memcpy(bp->b_data, buf, DEV_BSIZE);

	if (bwrite(bp) != 0) {
		DPRINTF("bwrite failed. block=%ld\n", (long)block);
		return false;
	}

	return true;
}

static void
v7fs_os_lock(void *self)
{

	mutex_enter((kmutex_t *)self);
}

static void
v7fs_os_unlock(void *self)
{

	mutex_exit((kmutex_t *)self);
}
