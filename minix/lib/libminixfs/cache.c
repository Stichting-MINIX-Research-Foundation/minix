
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
#include <minix/bitmap.h>

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

typedef struct buf *noxfer_buf_ptr_t; /* annotation for temporary buf ptrs */

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
  ASSERT(!bp->data);
  ASSERT(bp->lmfs_bytes == 0);

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
	assert(a);
	assert(a != MAP_FAILED);
	assert(!((vir_bytes)a % PAGE_SIZE));
	assert(len > 0);

	len = roundup(len, PAGE_SIZE);

	assert(!(len % PAGE_SIZE));

	if(munmap(a, len) < 0)
		panic("libminixfs cache: munmap failed");
}

static void raisecount(struct buf *bp)
{
  ASSERT(bp->lmfs_count < CHAR_MAX);
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
 * is PEEK, the function returns the block if it is in the cache or the VM
 * cache, and an ENOENT error code otherwise.
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

  /* The block is not found in our cache, but we do want it if it's in the VM
   * cache. The exception is NO_READ, purely for context switching performance
   * reasons. NO_READ is used for 1) newly allocated blocks, 2) blocks being
   * prefetched, and 3) blocks about to be fully overwritten. In the first two
   * cases, VM will not have the block in its cache anyway, and for the third
   * we save on one VM call only if the block is in the VM cache.
   */
  assert(!bp->data);
  assert(!bp->lmfs_bytes);
  if (how != NO_READ && vmcache) {
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

  if (how == NORMAL) {
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

  put_block(bp, 0);
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
	/* Aesthetics: do not report EOF errors on superblock reads, because
	 * this is a fairly common occurrence, e.g. during system installation.
	 */
	if (bp->lmfs_blocknr != 0 /*first block*/ || r != 0 /*EOF*/)
		printf("fs cache: I/O error on device %d/%d, block %"PRIu64
		    " (%zd)\n", major(dev), minor(dev), bp->lmfs_blocknr, r);

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
 *				sort_blocks				     *
 *===========================================================================*/
static void sort_blocks(struct buf **bufq, unsigned int bufqsize)
{
  struct buf *bp;
  int i, j, gap;

  gap = 1;
  do
	gap = 3 * gap + 1;
  while ((unsigned int)gap <= bufqsize);

  while (gap != 1) {
	gap /= 3;
	for (j = gap; (unsigned int)j < bufqsize; j++) {
		for (i = j - gap; i >= 0 &&
		    bufq[i]->lmfs_blocknr > bufq[i + gap]->lmfs_blocknr;
		    i -= gap) {
			bp = bufq[i];
			bufq[i] = bufq[i + gap];
			bufq[i + gap] = bp;
		}
	}
  }
}

/*===========================================================================*
 *				rw_scattered				     *
 *===========================================================================*/
static void rw_scattered(
  dev_t dev,			/* major-minor device number */
  struct buf **bufq,		/* pointer to array of buffers */
  unsigned int bufqsize,	/* number of buffers */
  int rw_flag			/* READING or WRITING */
)
{
/* Read or write scattered data from a device. */

  register struct buf *bp;
  register iovec_t *iop;
  static iovec_t iovec[NR_IOREQS];
  off_t pos;
  unsigned int i, iov_per_block;
#if !defined(NDEBUG)
  unsigned int start_in_use = bufs_in_use, start_bufqsize = bufqsize;
#endif /* !defined(NDEBUG) */

  if(bufqsize == 0) return;

#if !defined(NDEBUG)
  /* for READING, check all buffers on the list are obtained and held
   * (count > 0)
   */
  if (rw_flag == READING) {
	assert(bufqsize <= LMFS_MAX_PREFETCH);

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
#endif /* !defined(NDEBUG) */

  /* For WRITING, (Shell) sort buffers on lmfs_blocknr.
   * For READING, the buffers are already sorted.
   */
  if (rw_flag == WRITING)
	sort_blocks(bufq, bufqsize);

  /* Set up I/O vector and do I/O.  The result of bdev I/O is OK if everything
   * went fine, otherwise the error code for the first failed transfer.
   */
  while (bufqsize > 0) {
	unsigned int p, nblocks = 0, niovecs = 0;
	int r;
	for (iop = iovec; nblocks < bufqsize; nblocks++) {
		vir_bytes vdata, blockrem;
		bp = bufq[nblocks];
		if (bp->lmfs_blocknr != bufq[0]->lmfs_blocknr + nblocks)
			break;
		blockrem = bp->lmfs_bytes;
		iov_per_block = howmany(blockrem, PAGE_SIZE);
		if (niovecs > NR_IOREQS - iov_per_block) break;
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
	assert(niovecs > 0 && niovecs <= NR_IOREQS);

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
			bp = *bufq++;
			bp->lmfs_dev = NO_DEV;	/* invalidate block */
			lmfs_put_block(bp);
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

#if !defined(NDEBUG)
  if(rw_flag == READING) {
  	assert(start_in_use >= start_bufqsize);

	/* READING callers assume all bufs are released. */
	assert(start_in_use - start_bufqsize == bufs_in_use);
  }
#endif /* !defined(NDEBUG) */
}

/*===========================================================================*
 *				lmfs_readahead				     *
 *===========================================================================*/
void lmfs_readahead(dev_t dev, block64_t base_block, unsigned int nblocks,
	size_t last_size)
{
/* Read ahead 'nblocks' blocks starting from the block 'base_block' on device
 * 'dev'. The number of blocks must be between 1 and LMFS_MAX_PREFETCH,
 * inclusive. All blocks have the file system's block size, possibly except the
 * last block in the range, which is of size 'last_size'. The caller must
 * ensure that none of the blocks in the range are already in the cache.
 * However, the caller must also not rely on all or even any of the blocks to
 * be present in the cache afterwards--failures are (deliberately!) ignored.
 */
  static noxfer_buf_ptr_t bufq[LMFS_MAX_PREFETCH]; /* static for size only */
  struct buf *bp;
  unsigned int count;
  int r;

  assert(nblocks >= 1 && nblocks <= LMFS_MAX_PREFETCH);

  for (count = 0; count < nblocks; count++) {
	if (count == nblocks - 1)
		r = lmfs_get_partial_block(&bp, dev, base_block + count,
		    NO_READ, last_size);
	else
		r = lmfs_get_block(&bp, dev, base_block + count, NO_READ);

	if (r != OK)
		break;

	/* We could add a flag that makes the get_block() calls fail if the
	 * block is already in the cache, but it is not a major concern if it
	 * is: we just perform a useless read in that case. However, if the
	 * block is cached *and* dirty, we are about to lose its new contents.
	 */
	assert(lmfs_isclean(bp));

	bufq[count] = bp;
  }

  rw_scattered(dev, bufq, count, READING);
}

/*===========================================================================*
 *				lmfs_prefetch				     *
 *===========================================================================*/
unsigned int lmfs_readahead_limit(void)
{
/* Return the maximum number of blocks that should be read ahead at once. The
 * return value is guaranteed to be between 1 and LMFS_MAX_PREFETCH, inclusive.
 */
  unsigned int max_transfer, max_bufs;

  /* The returned value is the minimum of two factors: the maximum number of
   * blocks that can be transferred in a single I/O gather request (see how
   * rw_scattered() generates I/O requests), and a policy limit on the number
   * of buffers that any read-ahead operation may use (that is, thrash).
   */
  max_transfer = NR_IOREQS / MAX(fs_block_size / PAGE_SIZE, 1);

  /* The constants have been imported from MFS as is, and may need tuning. */
  if (nr_bufs < 50)
	max_bufs = 18;
  else
	max_bufs = nr_bufs - 4;

  return MIN(max_transfer, max_bufs);
}

/*===========================================================================*
 *				lmfs_prefetch				     *
 *===========================================================================*/
void lmfs_prefetch(dev_t dev, const block64_t *blockset, unsigned int nblocks)
{
/* The given set of blocks is expected to be needed soon, so prefetch a
 * convenient subset. The blocks are expected to be sorted by likelihood of
 * being accessed soon, making the first block of the set the most important
 * block to prefetch right now. The caller must have made sure that the blocks
 * are not in the cache already. The array may have duplicate block numbers.
 */
  bitchunk_t blocks_before[BITMAP_CHUNKS(LMFS_MAX_PREFETCH)];
  bitchunk_t blocks_after[BITMAP_CHUNKS(LMFS_MAX_PREFETCH)];
  block64_t block, base_block;
  unsigned int i, bit, nr_before, nr_after, span, limit, nr_blocks;

  if (nblocks == 0)
	return;

  /* Here is the deal. We are going to prefetch one range only, because seeking
   * is too expensive for just prefetching. The range we select should at least
   * include the first ("base") block of the given set, since that is the block
   * the caller is primarily interested in. Thus, the rest of the range is
   * going to have to be directly around this base block. We first check which
   * blocks from the set fall just before and after the base block, which then
   * allows us to construct a contiguous range of desired blocks directly
   * around the base block, in O(n) time. As a natural part of this, we ignore
   * duplicate blocks in the given set. We then read from the beginning of this
   * range, in order to maximize the chance that a next prefetch request will
   * continue from the last disk position without requiring a seek. However, we
   * do correct for the maximum number of blocks we can (or should) read in at
   * once, such that we will still end up reading the base block.
   */
  base_block = blockset[0];

  memset(blocks_before, 0, sizeof(blocks_before));
  memset(blocks_after, 0, sizeof(blocks_after));

  for (i = 1; i < nblocks; i++) {
	block = blockset[i];

	if (block < base_block && block + LMFS_MAX_PREFETCH >= base_block) {
		bit = base_block - block - 1;
		assert(bit < LMFS_MAX_PREFETCH);
		SET_BIT(blocks_before, bit);
	} else if (block > base_block &&
	    block - LMFS_MAX_PREFETCH <= base_block) {
		bit = block - base_block - 1;
		assert(bit < LMFS_MAX_PREFETCH);
		SET_BIT(blocks_after, bit);
	}
  }

  for (nr_before = 0; nr_before < LMFS_MAX_PREFETCH; nr_before++)
	if (!GET_BIT(blocks_before, nr_before))
		break;

  for (nr_after = 0; nr_after < LMFS_MAX_PREFETCH; nr_after++)
	if (!GET_BIT(blocks_after, nr_after))
		break;

  /* The number of blocks to prefetch is the minimum of two factors: the number
   * of blocks in the range around the base block, and the maximum number of
   * blocks that should be read ahead at once at all.
   */
  span = nr_before + 1 + nr_after;
  limit = lmfs_readahead_limit();

  nr_blocks = MIN(span, limit);
  assert(nr_blocks >= 1 && nr_blocks <= LMFS_MAX_PREFETCH);

  /* Start prefetching from the lowest block within the contiguous range, but
   * make sure that we read at least the original base block itself, too.
   */
  base_block -= MIN(nr_before, nr_blocks - 1);

  lmfs_readahead(dev, base_block, nr_blocks, fs_block_size);
}

/*===========================================================================*
 *				lmfs_flushdev				     *
 *===========================================================================*/
void lmfs_flushdev(dev_t dev)
{
/* Flush all dirty blocks for one device. */

  register struct buf *bp;
  static noxfer_buf_ptr_t *dirty;
  static unsigned int dirtylistsize = 0;
  unsigned int ndirty;

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

  rw_scattered(dev, dirty, ndirty, WRITING);
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
