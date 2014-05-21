
#define _SYSTEM

#include <assert.h>
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

#define BUFHASH(b) ((b) % nr_bufs)
#define MARKCLEAN  lmfs_markclean

#define MINBUFS 6 	/* minimal no of bufs for sanity check */

static struct buf *front;       /* points to least recently used free block */
static struct buf *rear;        /* points to most recently used free block */
static unsigned int bufs_in_use;/* # bufs currently in use (not on free list)*/

static void rm_lru(struct buf *bp);
static void read_block(struct buf *);
static void flushall(dev_t dev);
static void freeblock(struct buf *bp);
static void cache_heuristic_check(int major);

static int vmcache = 0; /* are we using vm's secondary cache? (initially not) */

static struct buf *buf;
static struct buf **buf_hash;   /* the buffer hash table */
static unsigned int nr_bufs;
static int may_use_vmcache;

static int fs_block_size = PAGE_SIZE;	/* raw i/o block size */

static int rdwt_err;

static int quiet = 0;

void lmfs_setquiet(int q) { quiet = q; }

static u32_t fs_bufs_heuristic(int minbufs, u32_t btotal, u64_t bfree, 
         int blocksize, dev_t majordev)
{
  struct vm_stats_info vsi;
  int bufs;
  u32_t kbytes_used_fs, kbytes_total_fs, kbcache, kb_fsmax;
  u32_t kbytes_remain_mem;
  u64_t bused;

  bused = btotal-bfree;

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

void lmfs_blockschange(dev_t dev, int delta)
{
        /* Change the number of allocated blocks by 'delta.'
         * Also accumulate the delta since the last cache re-evaluation.
         * If it is outside a certain band, ask the cache library to
         * re-evaluate the cache size.
         */
        static int bitdelta = 0;
        bitdelta += delta;
#define BANDKB (10*1024)	/* recheck cache every 10MB change */
        if(bitdelta*fs_block_size/1024 > BANDKB ||
	   bitdelta*fs_block_size/1024 < -BANDKB) {
                lmfs_cache_reevaluate(dev);
                bitdelta = 0;
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

int lmfs_bytes(struct buf *bp)
{
	return bp->lmfs_bytes;
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

static void lmfs_alloc_block(struct buf *bp)
{
  int len;
  ASSERT(!bp->data);
  ASSERT(bp->lmfs_bytes == 0);

  len = roundup(fs_block_size, PAGE_SIZE);

  if((bp->data = mmap(0, fs_block_size,
     PROT_READ|PROT_WRITE, MAP_PREALLOC|MAP_ANON, -1, 0)) == MAP_FAILED) {
	free_unused_blocks();
	if((bp->data = mmap(0, fs_block_size, PROT_READ|PROT_WRITE,
		MAP_PREALLOC|MAP_ANON, -1, 0)) == MAP_FAILED) {
		panic("libminixfs: could not allocate block");
	}
  }
  assert(bp->data);
  bp->lmfs_bytes = fs_block_size;
  bp->lmfs_needsetcache = 1;
}

/*===========================================================================*
 *				lmfs_get_block				     *
 *===========================================================================*/
struct buf *lmfs_get_block(register dev_t dev, register block_t block,
	int only_search)
{
	return lmfs_get_block_ino(dev, block, only_search, VMC_NO_INODE, 0);
}

void munmap_t(void *a, int len)
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
	if (!lmfs_isclean(bp)) flushall(bp->lmfs_dev);
	assert(bp->lmfs_bytes == fs_block_size);
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
 *				lmfs_get_block_ino			     *
 *===========================================================================*/
struct buf *lmfs_get_block_ino(dev_t dev, block_t block, int only_search,
	ino_t ino, u64_t ino_off)
{
/* Check to see if the requested block is in the block cache.  If so, return
 * a pointer to it.  If not, evict some other block and fetch it (unless
 * 'only_search' is 1).  All the blocks in the cache that are not in use
 * are linked together in a chain, with 'front' pointing to the least recently
 * used block and 'rear' to the most recently used block.  If 'only_search' is
 * 1, the block being requested will be overwritten in its entirety, so it is
 * only necessary to see if it is in the cache; if it is not, any free buffer
 * will do.  It is not necessary to actually read the block in from disk.
 * If 'only_search' is PREFETCH, the block need not be read from the disk,
 * and the device is not to be marked on the block, so callers can tell if
 * the block returned is valid.
 * In addition to the LRU chain, there is also a hash chain to link together
 * blocks whose block numbers end with the same bit strings, for fast lookup.
 */

  int b;
  static struct buf *bp;
  u64_t dev_off = (u64_t) block * fs_block_size;
  struct buf *prev_ptr;

  assert(buf_hash);
  assert(buf);
  assert(nr_bufs > 0);

  ASSERT(fs_block_size > 0);

  assert(dev != NO_DEV);

  if((ino_off % fs_block_size)) {

	printf("cache: unaligned lmfs_get_block_ino ino_off %llu\n",
		ino_off);
  	util_stacktrace();
  }

  /* Search the hash chain for (dev, block). */
  b = BUFHASH(block);
  bp = buf_hash[b];
  while (bp != NULL) {
  	if (bp->lmfs_blocknr == block && bp->lmfs_dev == dev) {
  		if(bp->lmfs_flags & VMMC_EVICTED) {
  			/* We had it but VM evicted it; invalidate it. */
  			ASSERT(bp->lmfs_count == 0);
  			ASSERT(!(bp->lmfs_flags & VMMC_BLOCK_LOCKED));
  			ASSERT(!(bp->lmfs_flags & VMMC_DIRTY));
  			bp->lmfs_dev = NO_DEV;
  			bp->lmfs_bytes = 0;
  			bp->data = NULL;
  			break;
  		}
  		/* Block needed has been found. */
  		if (bp->lmfs_count == 0) {
			rm_lru(bp);
			ASSERT(bp->lmfs_needsetcache == 0);
  			ASSERT(!(bp->lmfs_flags & VMMC_BLOCK_LOCKED));
			bp->lmfs_flags |= VMMC_BLOCK_LOCKED;
		}
		raisecount(bp);
  		ASSERT(bp->lmfs_bytes == fs_block_size);
  		ASSERT(bp->lmfs_dev == dev);
  		ASSERT(bp->lmfs_dev != NO_DEV);
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

  		return(bp);
  	} else {
  		/* This block is not the one sought. */
  		bp = bp->lmfs_hash; /* move to next block on hash chain */
  	}
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
		&bp->lmfs_flags, fs_block_size)) != MAP_FAILED) {
		bp->lmfs_bytes = fs_block_size;
		ASSERT(!bp->lmfs_needsetcache);
		return bp;
	}
  }
  bp->data = NULL;

  /* Not in the cache; reserve memory for its contents. */

  lmfs_alloc_block(bp);

  assert(bp->data);

  if(only_search == PREFETCH) {
	/* PREFETCH: don't do i/o. */
	bp->lmfs_dev = NO_DEV;
  } else if (only_search == NORMAL) {
	read_block(bp);
  } else if(only_search == NO_READ) {
  	/* This block will be overwritten by new contents. */
  } else
	panic("unexpected only_search value: %d", only_search);

  assert(bp->data);

  return(bp);			/* return the newly acquired block */
}

/*===========================================================================*
 *				lmfs_put_block				     *
 *===========================================================================*/
void lmfs_put_block(
  struct buf *bp,	/* pointer to the buffer to be released */
  int block_type 	/* INODE_BLOCK, DIRECTORY_BLOCK, or whatever */
)
{
/* Return a block to the list of available blocks.   Depending on 'block_type'
 * it may be put on the front or rear of the LRU chain.  Blocks that are
 * expected to be needed again shortly (e.g., partially full data blocks)
 * go on the rear; blocks that are unlikely to be needed again shortly
 * (e.g., full data blocks) go on the front.  Blocks whose loss can hurt
 * the integrity of the file system (e.g., inode blocks) are written to
 * disk immediately if they are dirty.
 */
  dev_t dev;
  off_t dev_off;
  int r;

  if (bp == NULL) return;	/* it is easier to check here than in caller */

  dev = bp->lmfs_dev;

  dev_off = (off_t) bp->lmfs_blocknr * fs_block_size;

  lowercount(bp);
  if (bp->lmfs_count != 0) return;	/* block is still in use */

  /* Put this block back on the LRU chain.  */
  if (dev == DEV_RAM || (block_type & ONE_SHOT)) {
	/* Block probably won't be needed quickly. Put it on front of chain.
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
	/* Block probably will be needed quickly.  Put it on rear of chain.
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

  /* block has sensible content - if necesary, identify it to VM */
  if(vmcache && bp->lmfs_needsetcache && dev != NO_DEV) {
  	if((r=vm_set_cacheblock(bp->data, dev, dev_off,
	bp->lmfs_inode, bp->lmfs_inode_offset,
	&bp->lmfs_flags, fs_block_size)) != OK) {
		if(r == ENOSYS) {
			printf("libminixfs: ENOSYS, disabling VM calls\n");
			vmcache = 0;
		} else {
			panic("libminixfs: setblock of %p dev 0x%llx off "
				"0x%llx failed\n", bp->data, dev, dev_off);
		}
	}
  }
  bp->lmfs_needsetcache = 0;

}

void lmfs_cache_reevaluate(dev_t dev)
{
  if(bufs_in_use == 0 && dev != NO_DEV) {
	/* if the cache isn't in use any more, we could resize it. */
	cache_heuristic_check(major(dev));
  }
}

/*===========================================================================*
 *				read_block				     *
 *===========================================================================*/
static void read_block(
  struct buf *bp	/* buffer pointer */
)
{
/* Read or write a disk block. This is the only routine in which actual disk
 * I/O is invoked. If an error occurs, a message is printed here, but the error
 * is not reported to the caller.  If the error occurred while purging a block
 * from the cache, it is not clear what the caller could do about it anyway.
 */
  int r, op_failed;
  off_t pos;
  dev_t dev = bp->lmfs_dev;

  op_failed = 0;

  assert(dev != NO_DEV);

  ASSERT(bp->lmfs_bytes == fs_block_size);
  ASSERT(fs_block_size > 0);

  pos = (off_t)bp->lmfs_blocknr * fs_block_size;
  if(fs_block_size > PAGE_SIZE) {
#define MAXPAGES 20
	vir_bytes blockrem, vaddr = (vir_bytes) bp->data;
	int p = 0;
  	static iovec_t iovec[MAXPAGES];
	blockrem = fs_block_size;
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
  	r = bdev_read(dev, pos, bp->data, fs_block_size,
  		BDEV_NOFLAGS);
  }
  if (r < 0) {
  	printf("fs cache: I/O error on device %d/%d, block %u\n",
  	major(dev), minor(dev), bp->lmfs_blocknr);
  	op_failed = 1;
  } else if (r != (ssize_t) fs_block_size) {
  	r = END_OF_FILE;
  	op_failed = 1;
  }

  if (op_failed) {
  	bp->lmfs_dev = NO_DEV;	/* invalidate block */

  	/* Report read errors to interested parties. */
  	rdwt_err = r;
  }

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

  vm_clear_cache(device);
}

/*===========================================================================*
 *				flushall				     *
 *===========================================================================*/
static void flushall(dev_t dev)
{
/* Flush all dirty blocks for one device. */

  register struct buf *bp;
  static struct buf **dirty;	/* static so it isn't on stack */
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
       if (!lmfs_isclean(bp) && bp->lmfs_dev == dev) {
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
  int start_in_use = bufs_in_use, start_bufqsize = bufqsize;

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
  iov_per_block = roundup(fs_block_size, PAGE_SIZE) / PAGE_SIZE;
  assert(iov_per_block < NR_IOREQS);
  
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
		if (bp->lmfs_blocknr != (block_t) bufq[0]->lmfs_blocknr + nblocks)
			break;
		if(niovecs >= NR_IOREQS-iov_per_block) break;
		vdata = (vir_bytes) bp->data;
		blockrem = fs_block_size;
		for(p = 0; p < iov_per_block; p++) {
			vir_bytes chunk = blockrem < PAGE_SIZE ? blockrem : PAGE_SIZE;
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
		printf("fs cache: I/O error %d on device %d/%d, block %u\n",
			r, major(dev), minor(dev), bufq[0]->lmfs_blocknr);
	}
	for (i = 0; i < nblocks; i++) {
		bp = bufq[i];
		if (r < (ssize_t) fs_block_size) {
			/* Transfer failed. */
			if (i == 0) {
				bp->lmfs_dev = NO_DEV;	/* Invalidate block */
			}
			break;
		}
		if (rw_flag == READING) {
			bp->lmfs_dev = dev;	/* validate block */
			lmfs_put_block(bp, PARTIAL_DATA_BLOCK);
		} else {
			MARKCLEAN(bp);
		}
		r -= fs_block_size;
	}

	bufq += i;
	bufqsize -= i;

	if (rw_flag == READING) {
		/* Don't bother reading more than the device is willing to
		 * give at this time.  Don't forget to release those extras.
		 */
		while (bufqsize > 0) {
			lmfs_put_block(*bufq++, PARTIAL_DATA_BLOCK);
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
static void cache_resize(unsigned int blocksize, unsigned int bufs)
{
  struct buf *bp;

  assert(blocksize > 0);
  assert(bufs >= MINBUFS);

  for (bp = &buf[0]; bp < &buf[nr_bufs]; bp++)
	if(bp->lmfs_count != 0) panic("change blocksize with buffer in use");

  lmfs_buf_pool(bufs);

  fs_block_size = blocksize;
}

static void cache_heuristic_check(int major)
{
  int bufs, d;
  u64_t btotal, bfree, bused;

  fs_blockstats(&btotal, &bfree, &bused);

  bufs = fs_bufs_heuristic(10, btotal, bfree,
        fs_block_size, major);

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
void lmfs_set_blocksize(int new_block_size, int major)
{
  cache_resize(new_block_size, MINBUFS);
  cache_heuristic_check(major);
  
  /* Decide whether to use seconday cache or not.
   * Only do this if
   *	- it's available, and
   *	- use of it hasn't been disabled for this fs, and
   *	- our main FS device isn't a memory device
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
	(void) fs_sync();
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
			flushall(bp->lmfs_dev);
}

int lmfs_fs_block_size(void)
{
	return fs_block_size;
}

void lmfs_may_use_vmcache(int ok)
{
	may_use_vmcache = ok;
}

void lmfs_reset_rdwt_err(void)
{
	rdwt_err = OK;
}

int lmfs_rdwt_err(void)
{
	return rdwt_err;
}

int lmfs_do_bpeek(message *m)
{
	block_t startblock, b, limitblock;
	dev_t dev = m->m_vfs_fs_breadwrite.device;
	off_t extra, pos = m->m_vfs_fs_breadwrite.seek_pos;
	size_t len = m->m_vfs_fs_breadwrite.nbytes;
	struct buf *bp;

	assert(m->m_type == REQ_BPEEK);
	assert(fs_block_size > 0);
	assert(dev != NO_DEV);

	if(!vmcache) { return ENXIO; }

	assert(!(fs_block_size % PAGE_SIZE));

	if((extra=(pos % fs_block_size))) {
		pos -= extra;
		len += extra;
	}

	len = roundup(len, fs_block_size);

	startblock = pos/fs_block_size;
	limitblock = startblock + len/fs_block_size;

	for(b = startblock; b < limitblock; b++) {
		bp = lmfs_get_block(dev, b, NORMAL);
		assert(bp);
		lmfs_put_block(bp, FULL_DATA_BLOCK);
	}

	return OK;
}
