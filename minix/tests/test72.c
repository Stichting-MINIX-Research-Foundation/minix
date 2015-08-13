/* Test 72 - libminixfs unit test.
 *
 * Exercise the caching functionality of libminixfs in isolation.
 */

#define _MINIX_SYSTEM

#include <minix/sysutil.h>
#include <minix/syslib.h>
#include <minix/vm.h>
#include <minix/bdev.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ioc_memory.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <minix/libminixfs.h>

int max_error = 0;

#include "common.h"
#include "testcache.h"

#define MYMAJOR	40	/* doesn't really matter, shouldn't be NO_DEV though */

#define MYDEV	makedev(MYMAJOR, 1)

static int curblocksize = -1;

static char *writtenblocks[MAXBLOCKS];

/* Some functions used by testcache.c */

int
dowriteblock(int b, int blocksize, u32_t seed, char *data)
{
	struct buf *bp;
	int r;

	assert(blocksize == curblocksize);

	if ((r = lmfs_get_block(&bp, MYDEV, b, NORMAL)) != 0) {
		e(30);
		return 0;
	}

	memcpy(bp->data, data, blocksize);

	lmfs_markdirty(bp);

	lmfs_put_block(bp);

	return blocksize;
}

int
readblock(int b, int blocksize, u32_t seed, char *data)
{
	struct buf *bp;
	int r;

	assert(blocksize == curblocksize);

	if ((r = lmfs_get_block(&bp, MYDEV, b, NORMAL)) != 0) {
		e(30);
		return 0;
	}

	memcpy(data, bp->data, blocksize);

	lmfs_put_block(bp);

	return blocksize;
}

void testend(void)
{
	int i;
	for(i = 0; i < MAXBLOCKS; i++) {
		if(writtenblocks[i]) {
			free(writtenblocks[i]);
			writtenblocks[i] = NULL;
		}
	}
}

/* Fake some libminixfs client functions */

static void allocate(int b)
{
	assert(curblocksize > 0);
	assert(!writtenblocks[b]);
	if(!(writtenblocks[b] = calloc(1, curblocksize))) {
		fprintf(stderr, "out of memory allocating block %d\n", b);
		exit(1);
	}
}

/* Fake some libblockdriver functions */
ssize_t
bdev_gather(dev_t dev, u64_t pos, iovec_t *vec, int count, int flags)
{
	int i, block;
	size_t size, block_off;
	ssize_t tot = 0;
	assert(dev == MYDEV);
	assert(curblocksize > 0);
	assert(!(pos % curblocksize));
	for(i = 0; i < count; i++) {
		char *data = (char *) vec[i].iov_addr;
		block = pos / curblocksize;
		block_off = (size_t)(pos % curblocksize);
		size = vec[i].iov_size;
		assert(size == PAGE_SIZE);
		assert(block >= 0);
		assert(block < MAXBLOCKS);
		assert(block_off + size <= curblocksize);
		if(!writtenblocks[block]) {
			allocate(block);
		}
		memcpy(data, writtenblocks[block] + block_off, size);
		pos += size;
		tot += size;
	}

	return tot;
}

ssize_t
bdev_scatter(dev_t dev, u64_t pos, iovec_t *vec, int count, int flags)
{
	int i, block;
	size_t size, block_off;
	ssize_t tot = 0;
	assert(dev == MYDEV);
	assert(curblocksize > 0);
	assert(!(pos % curblocksize));
	for(i = 0; i < count; i++) {
		char *data = (char *) vec[i].iov_addr;
		block = pos / curblocksize;
		block_off = (size_t)(pos % curblocksize);
		size = vec[i].iov_size;
		assert(size == PAGE_SIZE);
		assert(block >= 0);
		assert(block < MAXBLOCKS);
		assert(block_off + size <= curblocksize);
		if(!writtenblocks[block]) {
			allocate(block);
		}
		memcpy(writtenblocks[block] + block_off, data, size);
		pos += size;
		tot += size;
	}

	return tot;
}

ssize_t
bdev_read(dev_t dev, u64_t pos, char *data, size_t count, int flags)
{
	int block;

	assert(dev == MYDEV);
	assert(curblocksize > 0);
	assert(!(pos % curblocksize));
	assert(count > 0);
	assert(!(count % curblocksize));
	assert(count == PAGE_SIZE);
	assert(curblocksize == PAGE_SIZE);

	block = pos / curblocksize;
	assert(block >= 0);
	assert(block < MAXBLOCKS);
	if(!writtenblocks[block]) {
		allocate(block);
	}
	memcpy(data, writtenblocks[block], curblocksize);
	return count;
}

/* Fake some libsys functions */

__dead void
panic(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);

	exit(1);
}

int
vm_info_stats(struct vm_stats_info *vsi)
{
	return ENOSYS;
}

void
util_stacktrace(void)
{
	fprintf(stderr, "fake stacktrace\n");
}

void *alloc_contig(size_t len, int flags, phys_bytes *phys)
{
	return malloc(len);
}

int free_contig(void *addr, size_t len)
{
	free(addr);
	return 0;
}

u32_t sqrt_approx(u32_t v)
{
	return (u32_t) sqrt(v);
}

int vm_set_cacheblock(void *block, dev_t dev, off_t dev_offset,
        ino_t ino, off_t ino_offset, u32_t *flags, int blocksize, int setflags)
{
	return ENOSYS;
}

void *vm_map_cacheblock(dev_t dev, off_t dev_offset,
        ino_t ino, off_t ino_offset, u32_t *flags, int blocksize)
{
	return MAP_FAILED;
}

int vm_forget_cacheblock(dev_t dev, off_t dev_offset, int blocksize)
{
	return 0;
}

int vm_clear_cache(dev_t dev)
{
	return 0;
}

int
main(int argc, char *argv[])
{
	size_t newblocksize;
	int wss, cs, n = 0, p;

#define ITER 3
#define BLOCKS 200

	start(72);

	lmfs_setquiet(1);

	/* Can the cache handle differently sized blocks? */

	for(p = 1; p <= 3; p++) {
		/* Do not update curblocksize until the cache is flushed. */
		newblocksize = PAGE_SIZE*p;
		lmfs_set_blocksize(newblocksize);
		curblocksize = newblocksize;	/* now it's safe to update */
		lmfs_buf_pool(BLOCKS);
		if(dotest(curblocksize, BLOCKS, ITER)) e(n);
		n++;
	}
	
	/* Can the cache handle various combinations of the working set
	 * being larger and smaller than the cache?
	 */
	for(wss = 2; wss <= 3; wss++) {
		int wsblocks = 10*wss*wss*wss*wss*wss;
		for(cs = wsblocks/4; cs <= wsblocks*3; cs *= 1.5) {
			lmfs_set_blocksize(PAGE_SIZE);
			curblocksize = PAGE_SIZE;	/* same as above */
			lmfs_buf_pool(cs);
		        if(dotest(curblocksize, wsblocks, ITER)) e(n);
			n++;
		}
	}

	quit();

	return 0;
}

