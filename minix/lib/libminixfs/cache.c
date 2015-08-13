
#define _SYSTEM

#include <assert.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>

#include <machine/vmparam.h>

#include <sys/param.h>
#include <sys/mman.h>

#include <minix/dmap.h>
#include <minix/libminixfs.h>
#include <minix/syslib.h>
#include <minix/sysutil.h>
#include <minix/u64.h>
#include <minix/bdev.h>

#include "inc.h"

/* Buffer (block) cache.  To acquire a block, a routine calls lmfs_get_block(),
 * telling which block it wants.  The block is then regarded as "in use" and
 * has its reference count incremented.  All the blocks that are not in use are
 * chained together in an LRU list, with 'front' pointing to the least recently
 * used block, and 'rear' to the most recently used block.  A reverse chain is
 * also maintained.  Usage for LRU is measured by the time the put_block() is
 * done.  The second parameter to put_block() can violate the LRU order and put
 * a block on the front of the list, if it will probably not be needed again.
 * This is used internally only; the lmfs_put_block() API call has no second
 * parameter.  If a block is modified, the modifying routine must mark the
 * block as dirty, so the block will eventually be rewritten to the disk.
 */

/* Flags to put_block(). */
#define ONE_SHOT      0x1	/* set if block will not be needed again */

#define BUFHASH(b) ((unsigned int)((b) % nr_bufs))
#define MARKCLEAN  lmfs_markclean

#define MINBUFS 6 	/* minimal no of bufs for sanity check */

static struct buf *front;       /* points to least recently used free block */
static struct buf *rear;        /* points to most recently used free block */
static unsigned int bufs_in_use;/* # bufs currently in use (not on free list)*/

static void rm_lru(struct buf *bp);
static int read_block(struct buf *bp, size_t size);
static void freeblock(struct buf *bp);
static void cache_heuristic_check(void);
static void put_block(struct buf *bp, int put_flags);

static int vmcache = 0; /* are we using vm's secondary cache? (initially not) */

static struct buf *buf;
static struct buf **buf_hash;   /* the buffer hash table */
static unsigned int nr_bufs;
static int may_use_vmcache;

static size_t fs_block_size = PAGE_SIZE;	/* raw i/o block size */

static fsblkcnt_t fs_btotal = 0, fs_bused = 0;

static int quiet = 0;

void lmfs_setquiet(int q) { quiet = q; }

static int fs_bufs_heuristic(int minbufs, fsblkcnt_t btotal,
	fsblkcnt_t bused, int blocksize)
{
  struct vm_stats_info vsi;
  int bufs;
  u32_t kbytes_used_fs, kbytes_total_fs, kbcache, kb_fsmax;
  u32_t kbytes_remain_mem;

  /* set a reasonable cache size; cache at most a certain
   * portion of the used FS, and at most a certain %age of remaining
   * memory
   */
  if(vm_info_stats(&vsi) != OK) {
	bufs = 1024;
	if(!quiet)
	  printf("fslib: heuristic info fail: default to %d bufs\n", bufs);
	return bufs;
  }

  /* remaining free memory is unused memory plus memory in used for cache,
   * as the cache can be evicted
   */
  kbytes_remain_mem = (u64_t)(vsi.vsi_free + vsi.vsi_cached) *
	vsi.vsi_pagesize / 1024;

  /* check fs usage. */
  kbytes_used_fs  = (unsigned long)(((u64_t)bused * blocksize) / 1024);
  kbytes_total_fs = (unsigned long)(((u64_t)btotal * blocksize) / 1024);

  /* heuristic for a desired cache size based on FS usage;
   * but never bigger than half of the total filesystem
   */
  kb_fsmax = sqrt_approx(kbytes_used_fs)*40;
  kb_fsmax = MIN(kb_fsmax, kbytes_total_fs/2);

  /* heuristic for a maximum usage - 10% of remaining memory */
  kbcache = MIN(kbytes_remain_mem/10, kb_fsmax);
  bufs = kbcache * 1024 / blocksize;

  /* but we simply need MINBUFS no matter what */
  if(bufs < minbufs)
	bufs = minbufs;

  return bufs;
}

void lmfs_change_blockusage(int delta)
{
        /* Change the number of allocated blocks by 'delta.'
         * Also accumulate the delta since the last cache re-evaluation.
         * If it is outside a certain band, ask the cache library to
         * re-evaluate the cache size.
         */
        static int bitdelta = 0, warn_low = TRUE, warn_high = TRUE;

	/* Adjust the file system block usage counter accordingly. Do bounds
	 * checking, and report file system misbehavior.
	 */
	if (delta > 0 && (fsblkcnt_t)delta > fs_btotal - fs_bused) {
		if (warn_high) {
			printf("libminixfs: block usage overflow\n");
			warn_high = FALSE;
		}
		delta = (int)(fs_btotal - fs_bused);
	} else if (delta < 0 && (fsblkcnt_t)-delta > fs_bused) {
		if (warn_low) {
			printf("libminixfs: block usage underflow\n");
			warn_low = FALSE;
		}
		delta = -(int)fs_bused;
	}
	fs_bused += delta;

	bitdelta += delta;

#define BAND_KB (10*1024)	/* recheck cache every 10MB change */

	/* If the accumulated delta exceeds the configured threshold, resize
	 * the cache, but only if the cache isn't in use any more. In order to
	 * avoid that the latter case blocks a resize forever, we also call
	 * this function from lmfs_flushall(). Since lmfs_buf_pool() may call
	 * lmfs_flushall(), reset 'bitdelta' before doing the heuristics check.
	 */
	if (bufs_in_use == 0 &&
	    (bitdelta*(int)fs_block_size/1024 > BAND_KB ||
	    bitdelta*(int)fs_block_size/1024 < -BAND_KB)) {
		bitdelta = 0;
		cache_heuristic_check();
	}
}

void lmfs_markdirty(struct buf *bp)
{
	bp->lmfs_flags |= VMMC_DIRTY;
}

void lmfs_markclean(struct buf *bp)
{
	bp->lmfs_flags &= ~VMMC_DIRTY;
}

int lmfs_isclean(struct buf *bp)
{
	return !(bp->lmfs_flags & VMMC_DIRTY);
}

dev_t lmfs_dev(struct buf *bp)
{
	return bp->lmfs_dev;
}

static void free_unused_blocks(void)
{
	struct buf *bp;

	int freed = 0, bytes = 0;
	printf("libminixfs: freeing; %d blocks in use\n", bufs_in_use);
	for(bp = &buf[0]; bp < &buf[nr_bufs]; bp++) {
  		if(bp->lmfs_bytes > 0 && bp->lmfs_count == 0) {
			freed++;
			bytes += bp->lmfs_bytes;
			freeblock(bp);
		}
	}
	printf("libminixfs: freeing; %d blocks, %d bytes\n", freed, bytes);
}

static void lmfs_alloc_block(struct buf *bp, size_t block_size)
{
  int len;
  ASSERT(!bp->data);
  ASSERT(bp->lmfs_bytes == 0);

  len = roundup(block_size, PAGE_SIZE);

  if((bp->data = mmap(0, block_size, PROT_READ|PROT_WRITE,
      MAP_PREALLOC|MAP_ANON, -1, 0)) == MAP_FAILED) {
	free_unused_blocks();
	if((bp->data = mmap(0, block_size, PROT_READ|PROT_WRITE,
		MAP_PREALLOC|MAP_ANON, -1, 0)) == MAP_FAILED) {
		panic("libminixfs: could not allocate block");
	}
  }
  assert(bp->data);
  bp->lmfs_bytes = block_size;
  bp->lmfs_needsetcache = 1;
}

/*===========================================================================*
 *				lmfs_get_block				     *
 *===========================================================================*/
int lmfs_get_block(struct buf **bpp, dev_t dev, block64_t block, int how)
{
	return lmfs_get_block_ino(bpp, dev, block, how, VMC_NO_INODE, 0);
}

static void munmap_t(void *a, int len)
{
	vir_bytes av = (vir_bytes) a;
	assert(a);
	assert(a != MAP_FAILED);
	assert(len > 0);
	assert(!(av % PAGE_SIZE));

	len = roundup(len, PAGE_SIZE);

	assert(!(len % PAGE_SIZE));

	if(munmap(a, len) < 0)
		panic("libminixfs cache: munmap failed");
}

static void raisecount(struct buf *bp)
{
  assert(bufs_in_use >= 0);
  ASSERT(bp->lmfs_count >= 0);
  bp->lmfs_count++;
  if(bp->lmfs_count == 1) bufs_in_use++;
  assert(bufs_in_use > 0);
}

static void lowercount(struct buf *bp)
{
  assert(bufs_in_use > 0);
  ASSERT(bp->lmfs_count > 0);
  bp->lmfs_count--;
  if(bp->lmfs_count == 0) bufs_in_use--;
  assert(bufs_in_use >= 0);
}

static void freeblock(struct buf *bp)
{
  ASSERT(bp->lmfs_count == 0);
  /* If the block taken is dirty, make it clean by writing it to the disk.
   * Avoid hysteresis by flushing all other dirty blocks for the same device.
   */
  if (bp->lmfs_dev != NO_DEV) {
	if (!lmfs_isclean(bp)) lmfs_flushdev(bp->lmfs_dev);
	assert(bp->lmfs_bytes > 0);
	bp->lmfs_dev = NO_DEV;
  }

  /* Fill in block's parameters and add it to the hash chain where it goes. */
  MARKCLEAN(bp);		/* NO_DEV blocks may be marked dirty */
  if(bp->lmfs_bytes > 0) {
	assert(bp->data);
	munmap_t(bp->data, bp->lmfs_bytes);
	bp->lmfs_bytes = 0;
	bp->data = NULL;
  } else assert(!bp->data);
}

/*===========================================================================*
 *				find_block				     *
 *===========================================================================*/
static struct buf *find_block(dev_t dev, block64_t block)
{
/* Search the hash chain for (dev, block). Return the buffer structure if
 * found, or NULL otherwise.
 */
  struct buf *bp;
  int b;

  assert(dev != NO_DEV);

  b = BUFHASH(block);
  for (bp = buf_hash[b]; bp != NULL; bp = bp->lmfs_hash)
	if (bp->lmfs_blocknr == block && bp->lmfs_dev == dev)
		return bp;

  return NULL;
}

/*===========================================================================*
 *				get_block_ino				     *
 *===========================================================================*/
static int get_block_ino(struct buf **bpp, dev_t dev, block64_t block, int how,
	ino_t ino, u64_t ino_off, size_t block_size)
{
/* Check to see if the requested block is in the block cache.  The requested
 * block is identified by the block number in 'block' on device 'dev', counted
 * in the file system block size.  The amount of data requested for this block
 * is given in 'block_size', which may be less than the file system block size
 * iff the requested block is the last (partial) block on a device.  Note that
 * the given block size does *not* affect the conversion of 'block' to a byte
 * offset!  Either way, if the block could be obtained, either from the cache
 * or by reading from the device, return OK, with a pointer to the buffer
 * structure stored in 'bpp'.  If not, return a negative error code (and no
 * buffer).  If necessary, evict some other block and fetch the contents from
 * disk (if 'how' is NORMAL).  If 'how' is NO_READ, the caller intends to
 * overwrite the requested block in its entirety, so it is only necessary to
 * see if it is in the cache; if it is not, any free buffer will do.  If 'how'
 * is PREFETCH, the block need not be read from the disk, and the device is not
 * to be marked on the block (i.e., set to NO_DEV), so callers can tell if the
 * block returned is valid.  If 'how' is PEEK, the function returns the block
 * if it is in the cache or the VM cache, and an ENOENT error code otherwise.
 * In addition to the LRU chain, there is also a hash chain to link together
 * blocks whose block numbers end with the same bit strings, for fast lookup.
 */
  int b, r;
  static struct buf *bp;
  uint64_t dev_off;
  struct buf *prev_ptr;

  assert(buf_hash);
  assert(buf);
  assert(nr_bufs > 0);

  ASSERT(fs_block_size > 0);

  assert(dev != NO_DEV);

  assert(block <= UINT64_MAX / fs_block_size);

  dev_off = block * fs_block_size;

  if((ino_off % fs_block_size)) {

	printf("cache: unaligned lmfs_get_block_ino ino_off %llu\n",
		ino_off);
  	util_stacktrace();
  }

  /* See if the block is in the cache. If so, we can return it right away. */
  bp = find_block(dev, block);
  if (bp != NULL && !(bp->lmfs_flags & VMMC_EVICTED)) {
	ASSERT(bp->lmfs_dev == dev);
	ASSERT(bp->lmfs_dev != NO_DEV);

	/* The block must have exactly the requested number of bytes. */
	if (bp->lmfs_bytes != block_size)
		return EIO;

	/* Block needed has been found. */
	if (bp->lmfs_count == 0) {
		rm_lru(bp);
		ASSERT(bp->lmfs_needsetcache == 0);
		ASSERT(!(bp->lmfs_flags & VMMC_BLOCK_LOCKED));
		/* FIXME: race condition against the VMMC_EVICTED check */
		bp->lmfs_flags |= VMMC_BLOCK_LOCKED;
	}
	raisecount(bp);
	ASSERT(bp->lmfs_flags & VMMC_BLOCK_LOCKED);
	ASSERT(bp->data);

	if(ino != VMC_NO_INODE) {
		if(bp->lmfs_inode == VMC_NO_INODE
		|| bp->lmfs_inode != ino
		|| bp->lmfs_inode_offset != ino_off) {
			bp->lmfs_inode = ino;
			bp->lmfs_inode_offset = ino_off;
			bp->lmfs_needsetcache = 1;
		}
	}

	*bpp = bp;
	return OK;
  }

  /* We had the block in the cache but VM evicted it; invalidate it. */
  if (bp != NULL) {
	assert(bp->lmfs_flags & VMMC_EVICTED);
	ASSERT(bp->lmfs_count == 0);
	ASSERT(!(bp->lmfs_flags & VMMC_BLOCK_LOCKED));
	ASSERT(!(bp->lmfs_flags & VMMC_DIRTY));
	bp->lmfs_dev = NO_DEV;
	bp->lmfs_bytes = 0;
	bp->data = NULL;
  }

  /* Desired block is not on available chain. Find a free block to use. */
  if(bp) {
  	ASSERT(bp->lmfs_flags & VMMC_EVICTED);
  } else {
	if ((bp = front) == NULL) panic("all buffers in use: %d", nr_bufs);
  }
  assert(bp);

  rm_lru(bp);

  /* Remove the block that was just taken from its hash chain. */
  b = BUFHASH(bp->lmfs_blocknr);
  prev_ptr = buf_hash[b];
  if (prev_ptr == bp) {
	buf_hash[b] = bp->lmfs_hash;
  } else {
	/* The block just taken is not on the front of its hash chain. */
	while (prev_ptr->lmfs_hash != NULL)
		if (prev_ptr->lmfs_hash == bp) {
			prev_ptr->lmfs_hash = bp->lmfs_hash;	/* found it */
			break;
		} else {
			prev_ptr = prev_ptr->lmfs_hash;	/* keep looking */
		}
  }

  freeblock(bp);

  bp->lmfs_inode = ino;
  bp->lmfs_inode_offset = ino_off;

  bp->lmfs_flags = VMMC_BLOCK_LOCKED;
  bp->lmfs_needsetcache = 0;
  bp->lmfs_dev = dev;		/* fill in device number */
  bp->lmfs_blocknr = block;	/* fill in block number */
  ASSERT(bp->lmfs_count == 0);
  raisecount(bp);
  b = BUFHASH(bp->lmfs_blocknr);
  bp->lmfs_hash = buf_hash[b];

  buf_hash[b] = bp;		/* add to hash list */

  assert(dev != NO_DEV);

  /* Block is not found in our cache, but we do want it
   * if it's in the vm cache.
   */
  assert(!bp->data);
  assert(!bp->lmfs_bytes);
  if(vmcache) {
	if((bp->data = vm_map_cacheblock(dev, dev_off, ino, ino_off,
	    &bp->lmfs_flags, roundup(block_size, PAGE_SIZE))) != MAP_FAILED) {
		bp->lmfs_bytes = block_size;
		ASSERT(!bp->lmfs_needsetcache);
		*bpp = bp;
		return OK;
	}
  }
  bp->data = NULL;

  /* The block is not in the cache, and VM does not know about it. If we were
   * requested to search for the block only, we can now return failure to the
   * caller. Return the block to the pool without allocating data pages, since
   * these would be freed upon recycling the block anyway.
   */
  if (how == PEEK) {
	bp->lmfs_dev = NO_DEV;

	put_block(bp, ONE_SHOT);

	return ENOENT;
  }

  /* Not in the cache; reserve memory for its contents. */

  lmfs_alloc_block(bp, block_size);

  assert(bp->data);

  if(how == PREFETCH) {
	/* PREFETCH: don't do i/o. */
	bp->lmfs_dev = NO_DEV;
  } else if (how == NORMAL) {
	/* Try to read the block. Return an error code on failure. */
	if ((r = read_block(bp, block_size)) != OK) {
		put_block(bp, 0);

		return r;
	}
  } else if(how == NO_READ) {
  	/* This block will be overwritten by new contents. */
  } else
	panic("unexpected 'how' value: %d", how);

  assert(bp->data);

  *bpp = bp;			/* return the newly acquired block */
  return OK;
}

/*===========================================================================*
 *				lmfs_get_block_ino			     *
 *===========================================================================*/
int lmfs_get_block_ino(struct buf **bpp, dev_t dev, block64_t block, int how,
	ino_t ino, u64_t ino_off)
{
  return get_block_ino(bpp, dev, block, how, ino, ino_off, fs_block_size);
}

/*===========================================================================*
 *				lmfs_get_partial_block			     *
 *===========================================================================*/
int lmfs_get_partial_block(struct buf **bpp, dev_t dev, block64_t block,
	int how, size_t block_size)
{
  return get_block_ino(bpp, dev, block, how, VMC_NO_INODE, 0, block_size);
}

/*===========================================================================*
 *				put_block				     *
 *===========================================================================*/
static void put_block(struct buf *bp, int put_flags)
{
/* Return a block to the list of available blocks.   Depending on 'put_flags'
 * it may be put on the front or rear of the LRU chain.  Blocks that are
 * expected to be needed again at some point go on the rear; blocks that are
 * unlikely to be needed again at all go on the front.
 */
  dev_t dev;
  uint64_t dev_off;
  int r, setflags;

  assert(bp != NULL);

  dev = bp->lmfs_dev;

  dev_off = bp->lmfs_blocknr * fs_block_size;

  lowercount(bp);
  if (bp->lmfs_count != 0) return;	/* block is still in use */

  /* Put this block back on the LRU chain.  */
  if (dev == NO_DEV || dev == DEV_RAM || (put_flags & ONE_SHOT)) {
	/* Block will not be needed again. Put it on front of chain.
  	 * It will be the next block to be evicted from the cache.
  	 */
	bp->lmfs_prev = NULL;
	bp->lmfs_next = front;
	if (front == NULL)
		rear = bp;	/* LRU chain was empty */
	else
		front->lmfs_prev = bp;
	front = bp;
  } 
  else {
	/* Block may be needed again.  Put it on rear of chain.
  	 * It will not be evicted from the cache for a long time.
  	 */
	bp->lmfs_prev = rear;
	bp->lmfs_next = NULL;
	if (rear == NULL)
		front = bp;
	else
		rear->lmfs_next = bp;
	rear = bp;
  }

  assert(bp->lmfs_flags & VMMC_BLOCK_LOCKED);
  bp->lmfs_flags &= ~VMMC_BLOCK_LOCKED;

  /* block has sensible content - if necessary, identify it to VM */
  if(vmcache && bp->lmfs_needsetcache && dev != NO_DEV) {
	assert(bp->data);

	setflags = (put_flags & ONE_SHOT) ? VMSF_ONCE : 0;

	if ((r = vm_set_cacheblock(bp->data, dev, dev_off, bp->lmfs_inode,
	    bp->lmfs_inode_offset, &bp->lmfs_flags,
	    roundup(bp->lmfs_bytes, PAGE_SIZE), setflags)) != OK) {
		if(r == ENOSYS) {
			printf("libminixfs: ENOSYS, disabling VM calls\n");
			vmcache = 0;
		} else if (r == ENOMEM) {
			/* Do not panic in this case. Running out of memory is
			 * bad, especially since it may lead to applications
			 * crashing when trying to access memory-mapped pages
			 * we haven't been able to pass off to the VM cache,
			 * but the entire file system crashing is always worse.
			 */
			printf("libminixfs: no memory for cache block!\n");
		} else {
			panic("libminixfs: setblock of %p dev 0x%llx off "
				"0x%llx failed\n", bp->data, dev, dev_off);
		}
	}
  }
  bp->lmfs_needsetcache = 0;

  /* Now that we (may) have given the block to VM, invalidate the block if it
   * is a one-shot block.  Otherwise, it may still be reobtained immediately
   * after, which could be a problem if VM already forgot the block and we are
   * expected to pass it to VM again, which then wouldn't happen.
   */
  if (put_flags & ONE_SHOT)
	bp->lmfs_dev = NO_DEV;
}

/*===========================================================================*
 *				lmfs_put_block				     *
 *===========================================================================*/
void lmfs_put_block(struct buf *bp)
{
/* User interface to put_block(). */

  if (bp == NULL) return;	/* for poorly written file systems */

  return put_block(bp, 0);
}

/*===========================================================================*
 *				lmfs_free_block				     *
 *===========================================================================*/
void lmfs_free_block(dev_t dev, block64_t block)
{
/* The file system has just freed the given block. The block may previously
 * have been in use as data block for an inode. Therefore, we now need to tell
 * VM that the block is no longer associated with an inode. If we fail to do so
 * and the inode now has a hole at this location, mapping in the hole would
 * yield the old block contents rather than a zeroed page. In addition, if the
 * block is in the cache, it will be removed, even if it was dirty.
 */
  struct buf *bp;
  int r;

  /* Tell VM to forget about the block. The primary purpose of this call is to
   * break the inode association, but since the block is part of a mounted file
   * system, it is not expected to be accessed directly anyway. So, save some
   * cache memory by throwing it out of the VM cache altogether.
   */
  if (vmcache) {
	if ((r = vm_forget_cacheblock(dev, block * fs_block_size,
	    fs_block_size)) != OK)
		printf("libminixfs: vm_forget_cacheblock failed (%d)\n", r);
  }

  if ((bp = find_block(dev, block)) != NULL) {
	lmfs_markclean(bp);

	/* Invalidate the block. The block may or may not be in use right now,
	 * so don't be smart about freeing memory or repositioning in the LRU.
	 */
	bp->lmfs_dev = NO_DEV;
  }

  /* Note that this is *not* the right place to implement TRIM support. Even
   * though the block is freed, on the device it may still be part of a
   * previous checkpoint or snapshot of some sort. Only the file system can
   * be trusted to decide which blocks can be reused on the device!
   */
}

/*===========================================================================*
 *				lmfs_zero_block_ino			     *
 *===========================================================================*/
void lmfs_zero_block_ino(dev_t dev, ino_t ino, u64_t ino_off)
{
/* Files may have holes. From an application perspective, these are just file
 * regions filled with zeroes. From a file system perspective however, holes
 * may represent unallocated regions on disk. Thus, these holes do not have
 * corresponding blocks on the disk, and therefore also no block number.
 * Therefore, we cannot simply use lmfs_get_block_ino() for them. For reads,
 * this is not a problem, since the file system can just zero out the target
 * application buffer instead. For mapped pages however, this *is* a problem,
 * since the VM cache needs to be told about the corresponding block, and VM
 * does not accept blocks without a device offset. The role of this function is
 * therefore to tell VM about the hole using a fake device offset. The device
 * offsets are picked so that the VM cache will see a block memory-mapped for
 * the hole in the file, while the same block is not visible when
 * memory-mapping the block device.
 */
  struct buf *bp;
  static block64_t fake_block = 0;
  int r;

  if (!vmcache)
	return;

  assert(fs_block_size > 0);

  /* Pick a block number which is above the threshold of what can possibly be
   * mapped in by mmap'ing the device, since off_t is signed, and it is safe to
   * say that it will take a while before we have 8-exabyte devices. Pick a
   * different block number each time to avoid possible concurrency issues.
   * FIXME: it does not seem like VM actually verifies mmap offsets though..
   */
  if (fake_block == 0 || ++fake_block >= UINT64_MAX / fs_block_size)
	fake_block = ((uint64_t)INT64_MAX + 1) / fs_block_size;

  /* Obtain a block. */
  if ((r = lmfs_get_block_ino(&bp, dev, fake_block, NO_READ, ino,
      ino_off)) != OK)
	panic("libminixfs: getting a NO_READ block failed: %d", r);
  assert(bp != NULL);
  assert(bp->lmfs_dev != NO_DEV);

  /* The block is already zeroed, as it has just been allocated with mmap. File
   * systems do not rely on this assumption yet, so if VM ever gets changed to
   * not clear the blocks we allocate (e.g., by recycling pages in the VM cache
   * for the same process, which would be safe), we need to add a memset here.
   */

  /* Release the block. We don't expect it to be accessed ever again. Moreover,
   * if we keep the block around in the VM cache, it may erroneously be mapped
   * in beyond the file end later. Hence, use VMSF_ONCE when passing it to VM.
   * TODO: tell VM that it is an all-zeroes block, so that VM can deduplicate
   * all such pages in its cache.
   */
  put_block(bp, ONE_SHOT);
}

void lmfs_set_blockusage(fsblkcnt_t btotal, fsblkcnt_t bused)
{

  assert(bused <= btotal);
  fs_btotal = btotal;
  fs_bused = bused;

  /* if the cache isn't in use, we could resize it. */
  if (bufs_in_use == 0)
	cache_heuristic_check();
}

/*===========================================================================*
 *				read_block				     *
 *===========================================================================*/
static int read_block(struct buf *bp, size_t block_size)
{
/* Read a disk block of 'size' bytes.  The given size is always the FS block
 * size, except for the last block of a device.  If an I/O error occurs,
 * invalidate the block and return an error code.
 */
  ssize_t r;
  off_t pos;
  dev_t dev = bp->lmfs_dev;

  assert(dev != NO_DEV);

  ASSERT(bp->lmfs_bytes == block_size);
  ASSERT(fs_block_size > 0);

  pos = (off_t)bp->lmfs_blocknr * fs_block_size;
  if (block_size > PAGE_SIZE) {
#define MAXPAGES 20
	vir_bytes blockrem, vaddr = (vir_bytes) bp->data;
	int p = 0;
  	static iovec_t iovec[MAXPAGES];
	blockrem = block_size;
	while(blockrem > 0) {
		vir_bytes chunk = blockrem >= PAGE_SIZE ? PAGE_SIZE : blockrem;
		iovec[p].iov_addr = vaddr;
		iovec[p].iov_size = chunk;
		vaddr += chunk;
		blockrem -= chunk;
		p++;
	}
  	r = bdev_gather(dev, pos, iovec, p, BDEV_NOFLAGS);
  } else {
	r = bdev_read(dev, pos, bp->data, block_size, BDEV_NOFLAGS);
  }
  if (r != (ssize_t)block_size) {
	printf("fs cache: I/O error on device %d/%d, block %"PRIu64" (%zd)\n",
	    major(dev), minor(dev), bp->lmfs_blocknr, r);
	if (r >= 0)
		r = EIO; /* TODO: retry retrieving (just) the remaining part */

	bp->lmfs_dev = NO_DEV;	/* invalidate block */

	return r;
  }

  return OK;
}

/*===========================================================================*
 *				lmfs_invalidate				     *
 *===========================================================================*/
void lmfs_invalidate(
  dev_t device			/* device whose blocks are to be purged */
)
{
/* Remove all the blocks belonging to some device from the cache. */

  register struct buf *bp;

  assert(device != NO_DEV);

  for (bp = &buf[0]; bp < &buf[nr_bufs]; bp++) {
	if (bp->lmfs_dev == device) {
		assert(bp->data);
		assert(bp->lmfs_bytes > 0);
		munmap_t(bp->data, bp->lmfs_bytes);
		bp->lmfs_dev = NO_DEV;
		bp->lmfs_bytes = 0;
		bp->data = NULL;
	}
  }

  /* Clear the cache even if VM caching is disabled for the file system:
   * caching may be disabled as side effect of an error, leaving blocks behind
   * in the actual VM cache.
   */
  vm_clear_cache(device);
}

/*===========================================================================*
 *				lmfs_flushdev				     *
 *===========================================================================*/
void lmfs_flushdev(dev_t dev)
{
/* Flush all dirty blocks for one device. */

  register struct buf *bp;
  static struct buf **dirty;
  static unsigned int dirtylistsize = 0;
  int ndirty;

  if(dirtylistsize != nr_bufs) {
	if(dirtylistsize > 0) {
		assert(dirty != NULL);
		free(dirty);
	}
	if(!(dirty = malloc(sizeof(dirty[0])*nr_bufs)))
		panic("couldn't allocate dirty buf list");
	dirtylistsize = nr_bufs;
  }

  for (bp = &buf[0], ndirty = 0; bp < &buf[nr_bufs]; bp++) {
	/* Do not flush dirty blocks that are in use (lmfs_count>0): the file
	 * system may mark the block as dirty before changing its contents, in
	 * which case the new contents could end up being lost.
	 */
	if (!lmfs_isclean(bp) && bp->lmfs_dev == dev && bp->lmfs_count == 0) {
		dirty[ndirty++] = bp;
	}
  }

  lmfs_rw_scattered(dev, dirty, ndirty, WRITING);
}

/*===========================================================================*
 *				lmfs_rw_scattered			     *
 *===========================================================================*/
void lmfs_rw_scattered(
  dev_t dev,			/* major-minor device number */
  struct buf **bufq,		/* pointer to array of buffers */
  int bufqsize,			/* number of buffers */
  int rw_flag			/* READING or WRITING */
)
{
/* Read or write scattered data from a device. */

  register struct buf *bp;
  int gap;
  register int i;
  register iovec_t *iop;
  static iovec_t iovec[NR_IOREQS];
  off_t pos;
  int iov_per_block;
  unsigned int start_in_use = bufs_in_use, start_bufqsize = bufqsize;

  assert(bufqsize >= 0);
  if(bufqsize == 0) return;

  /* for READING, check all buffers on the list are obtained and held
   * (count > 0)
   */
  if (rw_flag == READING) {
	for(i = 0; i < bufqsize; i++) {
		assert(bufq[i] != NULL);
		assert(bufq[i]->lmfs_count > 0);
  	}

  	/* therefore they are all 'in use' and must be at least this many */
	assert(start_in_use >= start_bufqsize);
  }

  assert(dev != NO_DEV);
  assert(fs_block_size > 0);
  assert(howmany(fs_block_size, PAGE_SIZE) <= NR_IOREQS);
  
  /* (Shell) sort buffers on lmfs_blocknr. */
  gap = 1;
  do
	gap = 3 * gap + 1;
  while (gap <= bufqsize);
  while (gap != 1) {
  	int j;
	gap /= 3;
	for (j = gap; j < bufqsize; j++) {
		for (i = j - gap;
		     i >= 0 && bufq[i]->lmfs_blocknr > bufq[i + gap]->lmfs_blocknr;
		     i -= gap) {
			bp = bufq[i];
			bufq[i] = bufq[i + gap];
			bufq[i + gap] = bp;
		}
	}
  }

  /* Set up I/O vector and do I/O.  The result of bdev I/O is OK if everything
   * went fine, otherwise the error code for the first failed transfer.
   */
  while (bufqsize > 0) {
  	int nblocks = 0, niovecs = 0;
	int r;
	for (iop = iovec; nblocks < bufqsize; nblocks++) {
		int p;
		vir_bytes vdata, blockrem;
		bp = bufq[nblocks];
		if (bp->lmfs_blocknr != bufq[0]->lmfs_blocknr + nblocks)
			break;
		blockrem = bp->lmfs_bytes;
		iov_per_block = howmany(blockrem, PAGE_SIZE);
		if(niovecs >= NR_IOREQS-iov_per_block) break;
		vdata = (vir_bytes) bp->data;
		for(p = 0; p < iov_per_block; p++) {
			vir_bytes chunk =
			    blockrem < PAGE_SIZE ? blockrem : PAGE_SIZE;
			iop->iov_addr = vdata;
			iop->iov_size = chunk;
			vdata += PAGE_SIZE;
			blockrem -= chunk;
			iop++;
			niovecs++;
		}
		assert(p == iov_per_block);
		assert(blockrem == 0);
	}

	assert(nblocks > 0);
	assert(niovecs > 0);

	pos = (off_t)bufq[0]->lmfs_blocknr * fs_block_size;
	if (rw_flag == READING)
		r = bdev_gather(dev, pos, iovec, niovecs, BDEV_NOFLAGS);
	else
		r = bdev_scatter(dev, pos, iovec, niovecs, BDEV_NOFLAGS);

	/* Harvest the results.  The driver may have returned an error, or it
	 * may have done less than what we asked for.
	 */
	if (r < 0) {
		printf("fs cache: I/O error %d on device %d/%d, "
		    "block %"PRIu64"\n",
		    r, major(dev), minor(dev), bufq[0]->lmfs_blocknr);
	}
	for (i = 0; i < nblocks; i++) {
		bp = bufq[i];
		if (r < (ssize_t)bp->lmfs_bytes) {
			/* Transfer failed. */
			if (i == 0) {
				bp->lmfs_dev = NO_DEV;	/* Invalidate block */
			}
			break;
		}
		if (rw_flag == READING) {
			bp->lmfs_dev = dev;	/* validate block */
			lmfs_put_block(bp);
		} else {
			MARKCLEAN(bp);
		}
		r -= bp->lmfs_bytes;
	}

	bufq += i;
	bufqsize -= i;

	if (rw_flag == READING) {
		/* Don't bother reading more than the device is willing to
		 * give at this time.  Don't forget to release those extras.
		 */
		while (bufqsize > 0) {
			lmfs_put_block(*bufq++);
			bufqsize--;
		}
	}
	if (rw_flag == WRITING && i == 0) {
		/* We're not making progress, this means we might keep
		 * looping. Buffers remain dirty if un-written. Buffers are
		 * lost if invalidate()d or LRU-removed while dirty. This
		 * is better than keeping unwritable blocks around forever..
		 */
		break;
	}
  }

  if(rw_flag == READING) {
  	assert(start_in_use >= start_bufqsize);

	/* READING callers assume all bufs are released. */
	assert(start_in_use - start_bufqsize == bufs_in_use);
  }
}

/*===========================================================================*
 *				rm_lru					     *
 *===========================================================================*/
static void rm_lru(struct buf *bp)
{
/* Remove a block from its LRU chain. */
  struct buf *next_ptr, *prev_ptr;

  next_ptr = bp->lmfs_next;	/* successor on LRU chain */
  prev_ptr = bp->lmfs_prev;	/* predecessor on LRU chain */
  if (prev_ptr != NULL)
	prev_ptr->lmfs_next = next_ptr;
  else
	front = next_ptr;	/* this block was at front of chain */

  if (next_ptr != NULL)
	next_ptr->lmfs_prev = prev_ptr;
  else
	rear = prev_ptr;	/* this block was at rear of chain */
}

/*===========================================================================*
 *				cache_resize				     *
 *===========================================================================*/
static void cache_resize(size_t blocksize, unsigned int bufs)
{
  struct buf *bp;

  assert(blocksize > 0);
  assert(bufs >= MINBUFS);

  for (bp = &buf[0]; bp < &buf[nr_bufs]; bp++)
	if(bp->lmfs_count != 0) panic("change blocksize with buffer in use");

  lmfs_buf_pool(bufs);

  fs_block_size = blocksize;
}

static void cache_heuristic_check(void)
{
  int bufs, d;

  bufs = fs_bufs_heuristic(MINBUFS, fs_btotal, fs_bused, fs_block_size);

  /* set the cache to the new heuristic size if the new one
   * is more than 10% off from the current one.
   */
  d = bufs-nr_bufs;
  if(d < 0) d = -d;
  if(d*100/nr_bufs > 10) {
	cache_resize(fs_block_size, bufs);
  }
}

/*===========================================================================*
 *			lmfs_set_blocksize				     *
 *===========================================================================*/
void lmfs_set_blocksize(size_t new_block_size)
{
  cache_resize(new_block_size, MINBUFS);
  cache_heuristic_check();
  
  /* Decide whether to use seconday cache or not.
   * Only do this if the block size is a multiple of the page size, and using
   * the VM cache has been enabled for this FS.
   */

  vmcache = 0;

  if(may_use_vmcache && !(new_block_size % PAGE_SIZE))
	vmcache = 1;
}

/*===========================================================================*
 *                              lmfs_buf_pool                                *
 *===========================================================================*/
void lmfs_buf_pool(int new_nr_bufs)
{
/* Initialize the buffer pool. */
  register struct buf *bp;

  assert(new_nr_bufs >= MINBUFS);

  if(nr_bufs > 0) {
	assert(buf);
	lmfs_flushall();
  	for (bp = &buf[0]; bp < &buf[nr_bufs]; bp++) {
		if(bp->data) {
			assert(bp->lmfs_bytes > 0);
			munmap_t(bp->data, bp->lmfs_bytes);
		}
	}
  }

  if(buf)
	free(buf);

  if(!(buf = calloc(sizeof(buf[0]), new_nr_bufs)))
	panic("couldn't allocate buf list (%d)", new_nr_bufs);

  if(buf_hash)
	free(buf_hash);
  if(!(buf_hash = calloc(sizeof(buf_hash[0]), new_nr_bufs)))
	panic("couldn't allocate buf hash list (%d)", new_nr_bufs);

  nr_bufs = new_nr_bufs;

  bufs_in_use = 0;
  front = &buf[0];
  rear = &buf[nr_bufs - 1];

  for (bp = &buf[0]; bp < &buf[nr_bufs]; bp++) {
        bp->lmfs_blocknr = NO_BLOCK;
        bp->lmfs_dev = NO_DEV;
        bp->lmfs_next = bp + 1;
        bp->lmfs_prev = bp - 1;
        bp->data = NULL;
        bp->lmfs_bytes = 0;
  }
  front->lmfs_prev = NULL;
  rear->lmfs_next = NULL;

  for (bp = &buf[0]; bp < &buf[nr_bufs]; bp++) bp->lmfs_hash = bp->lmfs_next;
  buf_hash[0] = front;
}

int lmfs_bufs_in_use(void)
{
	return bufs_in_use;
}

int lmfs_nr_bufs(void)
{
	return nr_bufs;
}

void lmfs_flushall(void)
{
	struct buf *bp;
	for(bp = &buf[0]; bp < &buf[nr_bufs]; bp++)
		if(bp->lmfs_dev != NO_DEV && !lmfs_isclean(bp)) 
			lmfs_flushdev(bp->lmfs_dev);

	/* This is the moment where it is least likely (although certainly not
	 * impossible!) that there are buffers in use, since buffers should not
	 * be held across file system syncs. See if we already intended to
	 * resize the buffer cache, but couldn't. Be aware that we may be
	 * called indirectly from within lmfs_change_blockusage(), so care must
	 * be taken not to recurse infinitely. TODO: see if it is better to
	 * resize the cache from here *only*, thus guaranteeing a clean cache.
	 */
	lmfs_change_blockusage(0);
}

size_t lmfs_fs_block_size(void)
{
	return fs_block_size;
}

void lmfs_may_use_vmcache(int ok)
{
	may_use_vmcache = ok;
}
