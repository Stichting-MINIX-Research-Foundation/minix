/*	$NetBSD: rumpblk.c,v 1.60 2015/05/26 16:48:05 pooka Exp $	*/

/*
 * Copyright (c) 2009 Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by the
 * Finnish Cultural Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Block device emulation.  Presents a block device interface and
 * uses rumpuser system calls to satisfy I/O requests.
 *
 * We provide fault injection.  The driver can be made to fail
 * I/O occasionally.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rumpblk.c,v 1.60 2015/05/26 16:48:05 pooka Exp $");

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/condvar.h>
#include <sys/disklabel.h>
#include <sys/evcnt.h>
#include <sys/fcntl.h>
#include <sys/kmem.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/cprng.h>

#include <rump/rumpuser.h>

#include "rump_private.h"
#include "rump_vfs_private.h"

#if 0
#define DPRINTF(x) printf x
#else
#define DPRINTF(x)
#endif

#define RUMPBLK_SIZE 16
static struct rblkdev {
	char *rblk_path;
	int rblk_fd;
	int rblk_mode;

	uint64_t rblk_size;
	uint64_t rblk_hostoffset;
	uint64_t rblk_hostsize;
	int rblk_ftype;

	struct disklabel rblk_label;
} minors[RUMPBLK_SIZE];

static struct evcnt ev_io_total;
static struct evcnt ev_io_async;

static struct evcnt ev_bwrite_total;
static struct evcnt ev_bwrite_async;
static struct evcnt ev_bread_total;

dev_type_open(rumpblk_open);
dev_type_close(rumpblk_close);
dev_type_read(rumpblk_read);
dev_type_write(rumpblk_write);
dev_type_ioctl(rumpblk_ioctl);
dev_type_strategy(rumpblk_strategy);
dev_type_strategy(rumpblk_strategy_fail);
dev_type_dump(rumpblk_dump);
dev_type_size(rumpblk_size);

static const struct bdevsw rumpblk_bdevsw = {
	.d_open = rumpblk_open,
	.d_close = rumpblk_close,
	.d_strategy = rumpblk_strategy,
	.d_ioctl = rumpblk_ioctl,
	.d_dump = nodump,
	.d_psize = nosize,
	.d_discard = nodiscard,
	.d_flag = D_DISK
};

static const struct bdevsw rumpblk_bdevsw_fail = {
	.d_open = rumpblk_open,
	.d_close = rumpblk_close,
	.d_strategy = rumpblk_strategy_fail,
	.d_ioctl = rumpblk_ioctl,
	.d_dump = nodump,
	.d_psize = nosize,
	.d_discard = nodiscard,
	.d_flag = D_DISK
};

static const struct cdevsw rumpblk_cdevsw = {
	.d_open = rumpblk_open,
	.d_close = rumpblk_close,
	.d_read = rumpblk_read,
	.d_write = rumpblk_write,
	.d_ioctl = rumpblk_ioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_DISK
};

static int backend_open(struct rblkdev *, const char *);
static int backend_close(struct rblkdev *);

/* fail every n out of BLKFAIL_MAX */
#define BLKFAIL_MAX 10000
static int blkfail;
static unsigned randstate;
static kmutex_t rumpblk_lock;
static int sectshift = DEV_BSHIFT;

static void
makedefaultlabel(struct disklabel *lp, off_t size, int part)
{
	int i;

	memset(lp, 0, sizeof(*lp));

	lp->d_secperunit = size;
	lp->d_secsize = 1 << sectshift;
	lp->d_nsectors = size >> sectshift;
	lp->d_ntracks = 1;
	lp->d_ncylinders = 1;
	lp->d_secpercyl = lp->d_nsectors;

	/* oh dear oh dear */
	strncpy(lp->d_typename, "rumpd", sizeof(lp->d_typename));
	strncpy(lp->d_packname, "fictitious", sizeof(lp->d_packname));

	lp->d_type = DKTYPE_RUMPD;
	lp->d_rpm = 11;
	lp->d_interleave = 1;
	lp->d_flags = 0;

	/* XXX: RAW_PART handling? */
	for (i = 0; i < part; i++) {
		lp->d_partitions[i].p_fstype = FS_UNUSED;
	}
	lp->d_partitions[part].p_size = size >> sectshift;
	lp->d_npartitions = part+1;
	/* XXX: file system type? */

	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = 0; /* XXX */
}

int
rumpblk_init(void)
{
	char buf[64];
	devmajor_t rumpblkmaj = RUMPBLK_DEVMAJOR;
	unsigned tmp;
	int i;

	mutex_init(&rumpblk_lock, MUTEX_DEFAULT, IPL_NONE);

	if (rumpuser_getparam("RUMP_BLKFAIL", buf, sizeof(buf)) == 0) {
		blkfail = strtoul(buf, NULL, 10);
		/* fail everything */
		if (blkfail > BLKFAIL_MAX)
			blkfail = BLKFAIL_MAX;
		if (rumpuser_getparam("RUMP_BLKFAIL_SEED",
		    buf, sizeof(buf)) == 0) {
			randstate = strtoul(buf, NULL, 10);
		} else {
			randstate = cprng_fast32();
		}
		printf("rumpblk: FAULT INJECTION ACTIVE! fail %d/%d. "
		    "seed %u\n", blkfail, BLKFAIL_MAX, randstate);
	} else {
		blkfail = 0;
	}

	if (rumpuser_getparam("RUMP_BLKSECTSHIFT", buf, sizeof(buf)) == 0) {
		printf("rumpblk: ");
		tmp = strtoul(buf, NULL, 10);
		if (tmp >= DEV_BSHIFT)
			sectshift = tmp;
		else
			printf("RUMP_BLKSECTSHIFT must be least %d (now %d), ",
			   DEV_BSHIFT, tmp); 
		printf("using %d for sector shift (size %d)\n",
		    sectshift, 1<<sectshift);
	}

	memset(minors, 0, sizeof(minors));
	for (i = 0; i < RUMPBLK_SIZE; i++) {
		minors[i].rblk_fd = -1;
	}

	evcnt_attach_dynamic(&ev_io_total, EVCNT_TYPE_MISC, NULL,
	    "rumpblk", "I/O reqs");
	evcnt_attach_dynamic(&ev_io_async, EVCNT_TYPE_MISC, NULL,
	    "rumpblk", "async I/O");

	evcnt_attach_dynamic(&ev_bread_total, EVCNT_TYPE_MISC, NULL,
	    "rumpblk", "bytes read");
	evcnt_attach_dynamic(&ev_bwrite_total, EVCNT_TYPE_MISC, NULL,
	    "rumpblk", "bytes written");
	evcnt_attach_dynamic(&ev_bwrite_async, EVCNT_TYPE_MISC, NULL,
	    "rumpblk", "bytes written async");

	if (blkfail) {
		return devsw_attach("rumpblk",
		    &rumpblk_bdevsw_fail, &rumpblkmaj,
		    &rumpblk_cdevsw, &rumpblkmaj);
	} else {
		return devsw_attach("rumpblk",
		    &rumpblk_bdevsw, &rumpblkmaj,
		    &rumpblk_cdevsw, &rumpblkmaj);
	}
}

int
rumpblk_register(const char *path, devminor_t *dmin,
	uint64_t offset, uint64_t size)
{
	struct rblkdev *rblk;
	uint64_t flen;
	size_t len;
	int ftype, error, i;

	/* devices might not report correct size unless they're open */
	if ((error = rumpuser_getfileinfo(path, &flen, &ftype)) != 0)
		return error;

	/* verify host file is of supported type */
	if (!(ftype == RUMPUSER_FT_REG
	   || ftype == RUMPUSER_FT_BLK
	   || ftype == RUMPUSER_FT_CHR))
		return EINVAL;

	mutex_enter(&rumpblk_lock);
	for (i = 0; i < RUMPBLK_SIZE; i++) {
		if (minors[i].rblk_path&&strcmp(minors[i].rblk_path, path)==0) {
			mutex_exit(&rumpblk_lock);
			*dmin = i;
			return 0;
		}
	}

	for (i = 0; i < RUMPBLK_SIZE; i++)
		if (minors[i].rblk_path == NULL)
			break;
	if (i == RUMPBLK_SIZE) {
		mutex_exit(&rumpblk_lock);
		return EBUSY;
	}

	rblk = &minors[i];
	rblk->rblk_path = __UNCONST("taken");
	mutex_exit(&rumpblk_lock);

	len = strlen(path);
	rblk->rblk_path = malloc(len + 1, M_TEMP, M_WAITOK);
	strcpy(rblk->rblk_path, path);
	rblk->rblk_hostoffset = offset;
	if (size != RUMPBLK_SIZENOTSET) {
		KASSERT(size + offset <= flen);
		rblk->rblk_size = size;
	} else {
		KASSERT(offset < flen);
		rblk->rblk_size = flen - offset;
	}
	rblk->rblk_hostsize = flen;
	rblk->rblk_ftype = ftype;
	makedefaultlabel(&rblk->rblk_label, rblk->rblk_size, i);

	if ((error = backend_open(rblk, path)) != 0) {
		memset(&rblk->rblk_label, 0, sizeof(rblk->rblk_label));
		free(rblk->rblk_path, M_TEMP);
		rblk->rblk_path = NULL;
		return error;
	}

	*dmin = i;
	return 0;
}

/*
 * Unregister rumpblk.  It's the callers responsibility to make
 * sure it's no longer in use.
 */
int
rumpblk_deregister(const char *path)
{
	struct rblkdev *rblk;
	int i;

	mutex_enter(&rumpblk_lock);
	for (i = 0; i < RUMPBLK_SIZE; i++) {
		if (minors[i].rblk_path&&strcmp(minors[i].rblk_path, path)==0) {
			break;
		}
	}
	mutex_exit(&rumpblk_lock);

	if (i == RUMPBLK_SIZE)
		return ENOENT;

	rblk = &minors[i];
	backend_close(rblk);

	free(rblk->rblk_path, M_TEMP);
	memset(&rblk->rblk_label, 0, sizeof(rblk->rblk_label));
	rblk->rblk_path = NULL;

	return 0;
}

/*
 * Release all backend resources, to be called only when the rump
 * kernel is being shut down.
 * This routine does not do a full "fini" since we're going down anyway.
 */
void
rumpblk_fini(void)
{
	int i;

	for (i = 0; i < RUMPBLK_SIZE; i++) {
		struct rblkdev *rblk;

		rblk = &minors[i];
		if (rblk->rblk_fd != -1)
			backend_close(rblk);
	}
}

static int
backend_open(struct rblkdev *rblk, const char *path)
{
	int error, fd;

	KASSERT(rblk->rblk_fd == -1);
	error = rumpuser_open(path,
	    RUMPUSER_OPEN_RDWR | RUMPUSER_OPEN_BIO, &fd);
	if (error) {
		error = rumpuser_open(path,
		    RUMPUSER_OPEN_RDONLY | RUMPUSER_OPEN_BIO, &fd);
		if (error)
			return error;
		rblk->rblk_mode = FREAD;
	} else {
		rblk->rblk_mode = FREAD|FWRITE;
	}

	rblk->rblk_fd = fd;
	KASSERT(rblk->rblk_fd != -1);
	return 0;
}

static int
backend_close(struct rblkdev *rblk)
{

	rumpuser_close(rblk->rblk_fd);
	rblk->rblk_fd = -1;

	return 0;
}

int
rumpblk_open(dev_t dev, int flag, int fmt, struct lwp *l)
{
	struct rblkdev *rblk = &minors[minor(dev)];

	if (rblk->rblk_fd == -1)
		return ENXIO;

	if (((flag & (FREAD|FWRITE)) & ~rblk->rblk_mode) != 0) {
		return EACCES;
	}

	return 0;
}

int
rumpblk_close(dev_t dev, int flag, int fmt, struct lwp *l)
{

	return 0;
}

int
rumpblk_ioctl(dev_t dev, u_long xfer, void *addr, int flag, struct lwp *l)
{
	devminor_t dmin = minor(dev);
	struct rblkdev *rblk = &minors[dmin];
	struct partinfo *pi;
	int error = 0;

	/* well, me should support a few more, but we don't for now */
	switch (xfer) {
	case DIOCGDINFO:
		*(struct disklabel *)addr = rblk->rblk_label;
		break;

	case DIOCGPART:
		pi = addr;
		pi->part = &rblk->rblk_label.d_partitions[DISKPART(dmin)];
		pi->disklab = &rblk->rblk_label;
		break;

	/* it's synced enough along the write path */
	case DIOCCACHESYNC:
		break;

	case DIOCGMEDIASIZE:
		*(off_t *)addr = (off_t)rblk->rblk_size;
		break;

	default:
		error = ENOTTY;
		break;
	}

	return error;
}

static int
do_physio(dev_t dev, struct uio *uio, int which)
{
	void (*strat)(struct buf *);

	if (blkfail)
		strat = rumpblk_strategy_fail;
	else
		strat = rumpblk_strategy;

	return physio(strat, NULL, dev, which, minphys, uio);
}

int
rumpblk_read(dev_t dev, struct uio *uio, int flags)
{

	return do_physio(dev, uio, B_READ);
}

int
rumpblk_write(dev_t dev, struct uio *uio, int flags)
{

	return do_physio(dev, uio, B_WRITE);
}

static void
dostrategy(struct buf *bp)
{
	struct rblkdev *rblk = &minors[minor(bp->b_dev)];
	off_t off;
	int async = bp->b_flags & B_ASYNC;
	int op;

	if (bp->b_bcount % (1<<sectshift) != 0) {
		rump_biodone(bp, 0, EINVAL);
		return;
	}

	/* collect statistics */
	ev_io_total.ev_count++;
	if (async)
		ev_io_async.ev_count++;
	if (BUF_ISWRITE(bp)) {
		ev_bwrite_total.ev_count += bp->b_bcount;
		if (async)
			ev_bwrite_async.ev_count += bp->b_bcount;
	} else {
		ev_bread_total.ev_count++;
	}

	/*
	 * b_blkno is always in terms of DEV_BSIZE, and since we need
	 * to translate to a byte offset for the host read, this
	 * calculation does not need sectshift.
	 */
	off = bp->b_blkno << DEV_BSHIFT;

	/*
	 * Do bounds checking if we're working on a file.  Otherwise
	 * invalid file systems might attempt to read beyond EOF.  This
	 * is bad(tm) especially on mmapped images.  This is essentially
	 * the kernel bounds_check() routines.
	 */
	if (off + bp->b_bcount > rblk->rblk_size) {
		int64_t sz = rblk->rblk_size - off;

		/* EOF */
		if (sz == 0) {
			rump_biodone(bp, 0, 0);
			return;
		}
		/* beyond EOF ==> error */
		if (sz < 0) {
			rump_biodone(bp, 0, EINVAL);
			return;
		}

		/* truncate to device size */
		bp->b_bcount = sz;
	}

	off += rblk->rblk_hostoffset;
	DPRINTF(("rumpblk_strategy: 0x%x bytes %s off 0x%" PRIx64
	    " (0x%" PRIx64 " - 0x%" PRIx64 "), %ssync\n",
	    bp->b_bcount, BUF_ISREAD(bp) ? "READ" : "WRITE",
	    off, off, (off + bp->b_bcount), async ? "a" : ""));

	op = BUF_ISREAD(bp) ? RUMPUSER_BIO_READ : RUMPUSER_BIO_WRITE;
	if (BUF_ISWRITE(bp) && !async)
		op |= RUMPUSER_BIO_SYNC;

	rumpuser_bio(rblk->rblk_fd, op, bp->b_data, bp->b_bcount, off,
	    rump_biodone, bp);
}

void
rumpblk_strategy(struct buf *bp)
{

	dostrategy(bp);
}

/*
 * Simple random number generator.  This is private so that we can
 * very repeatedly control which blocks will fail.
 *
 * <mlelstv> pooka, rand()
 * <mlelstv> [paste]
 */
static unsigned
gimmerand(void)
{

	return (randstate = randstate * 1103515245 + 12345) % (0x80000000L);
}

/*
 * Block device with very simple fault injection.  Fails every
 * n out of BLKFAIL_MAX I/O with EIO.  n is determined by the env
 * variable RUMP_BLKFAIL.
 */
void
rumpblk_strategy_fail(struct buf *bp)
{

	if (gimmerand() % BLKFAIL_MAX >= blkfail) {
		dostrategy(bp);
	} else { 
		printf("block fault injection: failing I/O on block %lld\n",
		    (long long)bp->b_blkno);
		bp->b_error = EIO;
		biodone(bp);
	}
}
