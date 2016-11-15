/*	$NetBSD: flash_io.c,v 1.5 2014/02/25 18:30:09 pooka Exp $	*/

/*-
 * Copyright (c) 2011 Department of Software Engineering,
 *		      University of Szeged, Hungary
 * Copyright (c) 2011 Adam Hoka <ahoka@NetBSD.org>
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by the Department of Software Engineering, University of Szeged, Hungary
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: flash_io.c,v 1.5 2014/02/25 18:30:09 pooka Exp $");

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/kthread.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>

#include <dev/flash/flash.h>
#include <dev/flash/flash_io.h>

#ifdef FLASH_DEBUG
extern int flashdebug;
#endif

int flash_cachesync_timeout = 1;
int flash_cachesync_nodenum;

void flash_io_read(struct flash_io *, struct buf *);
void flash_io_write(struct flash_io *, struct buf *);
void flash_io_done(struct flash_io *, struct buf *, int);
int flash_io_cache_write(struct flash_io *, flash_addr_t, struct buf *);
void flash_io_cache_sync(struct flash_io *);

static int
flash_timestamp_diff(struct bintime *bt, struct bintime *b2)
{
	struct bintime b1 = *bt;
	struct timeval tv;

	bintime_sub(&b1, b2);
	bintime2timeval(&b1, &tv);

	return tvtohz(&tv);
}

static flash_addr_t
flash_io_getblock(struct flash_io *fio, struct buf *bp)
{
	flash_off_t block, last;

	/* get block number of first byte */
	block = bp->b_rawblkno * DEV_BSIZE / fio->fio_if->erasesize;

	/* block of the last bite */
	last = (bp->b_rawblkno * DEV_BSIZE + bp->b_resid - 1)
	    / fio->fio_if->erasesize;

	/* spans trough multiple blocks, needs special handling */
	if (last != block) {
		printf("0x%jx -> 0x%jx\n",
		    bp->b_rawblkno * DEV_BSIZE,
		    bp->b_rawblkno * DEV_BSIZE + bp->b_resid - 1);
		panic("TODO: multiple block write. last: %jd, current: %jd",
		    (intmax_t )last, (intmax_t )block);
	}

	return block;
}

int
flash_sync_thread_init(struct flash_io *fio, device_t dev,
    struct flash_interface *flash_if)
{
	int error;

	FLDPRINTF(("starting flash io thread\n"));

	fio->fio_dev = dev;
	fio->fio_if = flash_if;

	fio->fio_data = kmem_alloc(fio->fio_if->erasesize, KM_SLEEP);

	mutex_init(&fio->fio_lock, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&fio->fio_cv, "flashcv");

	error = bufq_alloc(&fio->fio_bufq, "fcfs", BUFQ_SORT_RAWBLOCK);
	if (error)
		goto err_bufq;

	fio->fio_exiting = false;
	fio->fio_write_pending = false;

	/* arrange to allocate the kthread */
	error = kthread_create(PRI_NONE, KTHREAD_MUSTJOIN | KTHREAD_MPSAFE,
	    NULL, flash_sync_thread, fio, &fio->fio_thread, "flashio");

	if (!error)
		return 0;

	bufq_free(fio->fio_bufq);
err_bufq:
	cv_destroy(&fio->fio_cv);
	mutex_destroy(&fio->fio_lock);
	kmem_free(fio->fio_data, fio->fio_if->erasesize);

	return error;
}

void
flash_sync_thread_destroy(struct flash_io *fio)
{
	FLDPRINTF(("stopping flash io thread\n"));

	mutex_enter(&fio->fio_lock);

	fio->fio_exiting = true;
	cv_broadcast(&fio->fio_cv);

	mutex_exit(&fio->fio_lock);

	kthread_join(fio->fio_thread);

	kmem_free(fio->fio_data, fio->fio_if->erasesize);
	bufq_free(fio->fio_bufq);
	mutex_destroy(&fio->fio_lock);
	cv_destroy(&fio->fio_cv);
}

int
flash_io_submit(struct flash_io *fio, struct buf *bp)
{
	FLDPRINTF(("submitting job to flash io thread: %p\n", bp));

	if (__predict_false(fio->fio_exiting)) {
		flash_io_done(fio, bp, ENODEV);
		return ENODEV;
	}

	if (BUF_ISREAD(bp)) {
		FLDPRINTF(("we have a read job\n"));

		mutex_enter(&fio->fio_lock);
		if (fio->fio_write_pending)
			flash_io_cache_sync(fio);
		mutex_exit(&fio->fio_lock);

		flash_io_read(fio, bp);
	} else {
		FLDPRINTF(("we have a write job\n"));

		flash_io_write(fio, bp);
	}
	return 0;
}

int
flash_io_cache_write(struct flash_io *fio, flash_addr_t block, struct buf *bp)
{
	size_t retlen;
	flash_addr_t base, offset;
	int error;

	KASSERT(mutex_owned(&fio->fio_lock));
	KASSERT(fio->fio_if->erasesize != 0);

	base = block * fio->fio_if->erasesize;
	offset = bp->b_rawblkno * DEV_BSIZE - base;

	FLDPRINTF(("io cache write, offset: %jd\n", (intmax_t )offset));

	if (!fio->fio_write_pending) {
		fio->fio_block = block;
		/*
		 * fill the cache with data from flash,
		 * so we dont have to bother with gaps later
		 */
		FLDPRINTF(("filling buffer from offset %ju\n", (uintmax_t)base));
		error = fio->fio_if->read(fio->fio_dev,
		    base, fio->fio_if->erasesize,
		    &retlen, fio->fio_data);
		FLDPRINTF(("cache filled\n"));

		if (error)
			return error;

		fio->fio_write_pending = true;
		/* save creation time for aging */
		binuptime(&fio->fio_creation);
	}
	/* copy data to cache */
	memcpy(fio->fio_data + offset, bp->b_data, bp->b_resid);
	bufq_put(fio->fio_bufq, bp);

	/* update timestamp */
	binuptime(&fio->fio_last_write);

	return 0;
}

void
flash_io_cache_sync(struct flash_io *fio)
{
	struct flash_erase_instruction ei;
	struct buf *bp;
	size_t retlen;
	flash_addr_t base;
	int error;

	KASSERT(mutex_owned(&fio->fio_lock));

	if (!fio->fio_write_pending) {
		FLDPRINTF(("trying to sync with an invalid buffer\n"));
		return;
	}

	base = fio->fio_block * fio->fio_if->erasesize;

	FLDPRINTF(("eraseing block at 0x%jx\n", (uintmax_t )base));
	ei.ei_addr = base;
	ei.ei_len = fio->fio_if->erasesize;
	ei.ei_callback = NULL;
	error = fio->fio_if->erase(fio->fio_dev, &ei);

	if (error) {
		aprint_error_dev(fio->fio_dev, "cannot erase flash flash!\n");
		goto out;
	}

	FLDPRINTF(("writing %" PRIu32 " bytes to 0x%jx\n",
		fio->fio_if->erasesize, (uintmax_t )base));

	error = fio->fio_if->write(fio->fio_dev,
	    base, fio->fio_if->erasesize, &retlen, fio->fio_data);

	if (error || retlen != fio->fio_if->erasesize) {
		aprint_error_dev(fio->fio_dev, "can't sync write cache: %d\n", error);
		goto out;
	}

out:
	while ((bp = bufq_get(fio->fio_bufq)) != NULL)
		flash_io_done(fio, bp, error);

	fio->fio_block = -1;
	fio->fio_write_pending = false;
}

void
flash_sync_thread(void * arg)
{
	struct flash_io *fio = arg;
	struct bintime now;

	mutex_enter(&fio->fio_lock);

	while (!fio->fio_exiting) {
		cv_timedwait_sig(&fio->fio_cv, &fio->fio_lock, hz / 4);
		if (!fio->fio_write_pending) {
			continue;
		}
		/* see if the cache is older than 3 seconds (safety limit),
		 * or if we havent touched the cache since more than 1 ms
		 */
		binuptime(&now);
		if (flash_timestamp_diff(&now, &fio->fio_last_write) > hz / 5) {
			FLDPRINTF(("syncing write cache after timeout\n"));
			flash_io_cache_sync(fio);
		} else if (flash_timestamp_diff(&now, &fio->fio_creation)
		    > 3 * hz) {
			aprint_error_dev(fio->fio_dev,
			    "syncing write cache after 3 sec timeout!\n");
			flash_io_cache_sync(fio);
		}
	}

	mutex_exit(&fio->fio_lock);

	kthread_exit(0);
}

void
flash_io_read(struct flash_io *fio, struct buf *bp)
{
	size_t retlen;
	flash_addr_t offset;
	int error;

	FLDPRINTF(("flash io read\n"));

	offset = bp->b_rawblkno * DEV_BSIZE;

	error = fio->fio_if->read(fio->fio_dev, offset, bp->b_resid,
	    &retlen, bp->b_data);

	flash_io_done(fio, bp, error);
}

void
flash_io_write(struct flash_io *fio, struct buf *bp)
{
	flash_addr_t block;

	FLDPRINTF(("flash io write\n"));

	block = flash_io_getblock(fio, bp);
	FLDPRINTF(("write to block %jd\n", (intmax_t )block));

	mutex_enter(&fio->fio_lock);

	if (fio->fio_write_pending && fio->fio_block != block) {
		FLDPRINTF(("writing to new block, syncing caches\n"));
		flash_io_cache_sync(fio);
	}

	flash_io_cache_write(fio, block, bp);

	mutex_exit(&fio->fio_lock);
}

void
flash_io_done(struct flash_io *fio, struct buf *bp, int error)
{
	FLDPRINTF(("io done: %p\n", bp));

	if (error == 0)
		bp->b_resid = 0;

	bp->b_error = error;
	biodone(bp);
}

static int
sysctl_flash_verify(SYSCTLFN_ARGS)
{
	int error, t;
	struct sysctlnode node;

	node = *rnode;
	t = *(int *)rnode->sysctl_data;
	node.sysctl_data = &t;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	if (node.sysctl_num == flash_cachesync_nodenum) {
		if (t <= 0 || t > 60)
			return EINVAL;
	} else {
		return EINVAL;
	}

	*(int *)rnode->sysctl_data = t;

	return 0;
}

SYSCTL_SETUP(sysctl_flash, "sysctl flash subtree setup")
{
	int rc, flash_root_num;
	const struct sysctlnode *node;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT, CTLTYPE_NODE, "flash",
	    SYSCTL_DESCR("FLASH driver controls"),
	    NULL, 0, NULL, 0, CTL_HW, CTL_CREATE, CTL_EOL)) != 0) {
		goto error;
	}

	flash_root_num = node->sysctl_num;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
	    CTLTYPE_INT, "cache_sync_timeout",
	    SYSCTL_DESCR("FLASH write cache sync timeout in seconds"),
	    sysctl_flash_verify, 0, &flash_cachesync_timeout,
	    0, CTL_HW, flash_root_num, CTL_CREATE,
	    CTL_EOL)) != 0) {
		goto error;
	}

	flash_cachesync_nodenum = node->sysctl_num;

	return;

error:
	aprint_error("%s: sysctl_createv failed (rc = %d)\n", __func__, rc);
}
