/*
 * This file provides an implementation for block I/O functions as expected by
 * libfsdriver for root file systems.  In particular, the lmfs_driver function
 * can be used to implement fdr_driver, the lmfs_bio function can be used to
 * implement the fdr_bread, fdr_bwrite, and fdr_bpeek hooks, and the the
 * lmfs_bflush function can be used to implement the fdr_bflush hook.  At the
 * very least, a file system that makes use of the provided functionality
 * must adhere to the following rules:
 *
 *   o  it must initialize this library in order to set up a buffer pool for
 *      use by these functions, using the lmfs_buf_pool function; the
 *      recommended number of blocks for *non*-disk-backed file systems is
 *      LMFS_MAX_PREFETCH buffers (disk-backed file systems typically use many
 *      more);
 *   o  it must enable VM caching in order to support memory mapping of block
 *      devices, using the lmfs_may_use_vmcache function;
 *   o  it must either use lmfs_flushall as implementation for the fdr_sync
 *      hook, or call lmfs_flushall as part of its own fdr_sync implementation.
 *
 * In addition, a disk-backed file system (as opposed to e.g. a networked file
 * system that intends to be able to serve as a root file system) should
 * consider the following points:
 *
 *   o  it may restrict calls to fdr_bwrite on the mounted partition, for
 *      example to the partition's first 1024 bytes; it should generally not
 *      prevent that area from being written even if the file system is mounted
 *      read-only;
 *   o  it is free to set its own block size, although the default block size
 *      works fine for raw block I/O as well.
 */

#include <minix/drivers.h>
#include <minix/libminixfs.h>
#include <minix/fsdriver.h>
#include <minix/bdev.h>
#include <minix/partition.h>
#include <sys/ioctl.h>
#include <assert.h>

#include "inc.h"

/*
 * Set the driver label of the device identified by 'dev' to 'label'.  While
 * 'dev' is a full device number, only its major device number is to be used.
 * This is a very thin wrapper right now, but eventually we will want to hide
 * all of libbdev from file systems that use this library, so it is a start.
 */
void
lmfs_driver(dev_t dev, char *label)
{

	bdev_driver(dev, label);
}

/*
 * Prefetch up to "nblocks" blocks on "dev" starting from block number "block".
 * The size to be used for the last block in the range is given as "last_size".
 * Stop early when either the I/O request fills up or when a block is already
 * found to be in the cache.  The latter is likely to happen often, since this
 * function is called before getting each block for reading.  Prefetching is a
 * strictly best-effort operation, and may fail silently.
 * TODO: limit according to the number of available buffers.
 */
static void
block_prefetch(dev_t dev, block64_t block, unsigned int nblocks,
	size_t block_size, size_t last_size)
{
	struct buf *bp;
	unsigned int count, limit;
	int r;

	limit = lmfs_readahead_limit();
	assert(limit >= 1 && limit <= LMFS_MAX_PREFETCH);

	if (nblocks > limit) {
		nblocks = limit;

		last_size = block_size;
	}

	for (count = 0; count < nblocks; count++) {
		if (count == nblocks - 1 && last_size < block_size)
			r = lmfs_get_partial_block(&bp, dev, block + count,
			    PEEK, last_size);
		else
			r = lmfs_get_block(&bp, dev, block + count, PEEK);

		if (r == OK) {
			lmfs_put_block(bp);

			last_size = block_size;

			break;
		}
	}

	if (count > 0)
		lmfs_readahead(dev, block, count, last_size);
}

/*
 * Perform block I/O, on "dev", starting from offset "pos", for a total of
 * "bytes" bytes.  Reading, writing, and peeking are highly similar, and thus,
 * this function implements all of them.  The "call" parameter indicates the
 * call type (one of FSC_READ, FSC_WRITE, FSC_PEEK).  For read and write calls,
 * "data" will identify the user buffer to use; for peek calls, "data" is set
 * to NULL.  In all cases, this function returns the number of bytes
 * successfully transferred, 0 on end-of-file conditions, and a negative error
 * code if no bytes could be transferred due to an error.  Dirty data is not
 * flushed immediately, and thus, a successful write only indicates that the
 * data have been taken in by the cache (for immediate I/O, a character device
 * would have to be used, but MINIX3 no longer supports this), which may be
 * follwed later by silent failures.  End-of-file conditions are always
 * reported immediately, though.
 */
ssize_t
lmfs_bio(dev_t dev, struct fsdriver_data * data, size_t bytes, off_t pos,
	int call)
{
	block64_t block;
	struct part_geom part;
	size_t block_size, off, block_off, last_size, size, chunk;
	unsigned int blocks_left;
	struct buf *bp;
	int r, do_write, how;

	if (dev == NO_DEV)
		return EINVAL;

	block_size = lmfs_fs_block_size();
	do_write = (call == FSC_WRITE);

	assert(block_size > 0);

	if (bytes == 0)
		return 0; /* just in case */

	if (pos < 0 || bytes > SSIZE_MAX || pos > INT64_MAX - bytes + 1)
		return EINVAL;

	/*
	 * Get the partition size, so that we can handle EOF ourselves.
	 * Unfortunately, we cannot cache the results between calls, since we
	 * do not get to see DIOCSETP ioctls--see also repartition(8).
	 */
	if ((r = bdev_ioctl(dev, DIOCGETP, &part, NONE /*user_endpt*/)) != OK)
		return r;

	if ((uint64_t)pos >= part.size)
		return 0; /* EOF */

	if ((uint64_t)pos > part.size - bytes)
		bytes = part.size - pos;

	off = 0;
	block = pos / block_size;
	block_off = (size_t)(pos % block_size);
	blocks_left = howmany(block_off + bytes, block_size);

	assert(blocks_left > 0);

	/*
	 * If the last block we need is also the last block of the device,
	 * see how many bytes we should actually transfer for that block.
	 */
	if (block + blocks_left - 1 == part.size / block_size)
		last_size = part.size % block_size;
	else
		last_size = block_size;

	r = OK;

	for (off = 0; off < bytes && blocks_left > 0; off += chunk) {
		size = (blocks_left == 1) ? last_size : block_size;

		chunk = size - block_off;
		if (chunk > bytes - off)
			chunk = bytes - off;

		assert(chunk > 0 && chunk <= size);

		/*
		 * For read requests, help the block driver form larger I/O
		 * requests.
		 */
		if (!do_write)
			block_prefetch(dev, block, blocks_left, block_size,
			    last_size);

		/*
		 * Do not read the block from disk if we will end up
		 * overwriting all of its contents.
		 */
		how = (do_write && chunk == size) ? NO_READ : NORMAL;

		if (size < block_size)
			r = lmfs_get_partial_block(&bp, dev, block, how, size);
		else
			r = lmfs_get_block(&bp, dev, block, how);

		if (r != OK) {
			printf("libminixfs: error getting block <%"PRIx64","
			    "%"PRIu64"> for device I/O (%d)\n", dev, block, r);

			break;
		}

		/* Perform the actual copy. */
		if (r == OK && data != NULL) {
			if (do_write) {
				r = fsdriver_copyin(data, off,
				    (char *)bp->data + block_off, chunk);

				/*
				 * Mark the block as dirty even if the copy
				 * failed, since the copy may in fact have
				 * succeeded partially.  This is an interface
				 * issue that should be resolved at some point,
				 * but for now we do not want the cache to be
				 * desynchronized from the disk contents.
				 */
				lmfs_markdirty(bp);
			} else
				r = fsdriver_copyout(data, off,
				    (char *)bp->data + block_off, chunk);
		}

		lmfs_put_block(bp);

		if (r != OK)
			break;

		block++;
		block_off = 0;
		blocks_left--;
	}

	/*
	 * If we were not able to do any I/O, return the error.  Otherwise,
	 * return how many bytes we did manage to transfer.
	 */
	if (r != OK && off == 0)
		return r;

	return off;
}

/*
 * Perform a flush request on a block device, flushing and invalidating all
 * blocks associated with this device, both in the local cache and in VM.
 * This operation is called after a block device is closed and must prevent
 * that stale copies of blocks remain in any cache.
 */
void
lmfs_bflush(dev_t dev)
{

	/* First flush any dirty blocks on this device to disk. */
	lmfs_flushdev(dev);

	/* Then purge any blocks associated with the device. */
	lmfs_invalidate(dev);
}
