
#define _SYSTEM

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>

#include <sys/param.h>
#include <sys/param.h>

#include <minix/dmap.h>
#include <minix/libminixfs.h>
#include <minix/syslib.h>
#include <minix/sysutil.h>
#include <minix/u64.h>
#include <minix/bdev.h>

#define BP_CLEAN        0       /* on-disk block and memory copies identical */
#define BP_DIRTY        1       /* on-disk block and memory copies differ */

#define BUFHASH(b) ((b) % nr_bufs)
#define MARKCLEAN  lmfs_markclean

#define MINBUFS 6 	/* minimal no of bufs for sanity check */

static struct buf *front;       /* points to least recently used free block */
static struct buf *rear;        /* points to most recently used free block */
static unsigned int bufs_in_use;/* # bufs currently in use (not on free list)*/

static void rm_lru(struct buf *bp);
static void read_block(struct buf *);
static void flushall(dev_t dev);

static int vmcache = 0; /* are we using vm's secondary cache? (initially not) */

static struct buf *buf;
static struct buf **buf_hash;   /* the buffer hash table */
static unsigned int nr_bufs;
static int may_use_vmcache;

static int fs_block_size = 1024;	/* raw i/o block size */

static int rdwt_err;

u32_t fs_bufs_heuristic(int minbufs, u32_t btotal, u32_t bfree, 
         int blocksize, dev_t majordev)
{
  struct vm_stats_info vsi;
  int bufs;
  u32_t kbytes_used_fs, kbytes_total_fs, kbcache, kb_fsmax;
  u32_t kbytes_remain_mem, bused;

  bused = btotal-bfree;

  /* but we simply need minbufs no matter what, and we don't
   * want more than that if we're a memory device
   */
  if(majordev == MEMORY_MAJOR) {
	return minbufs;
  }

  /* set a reasonable cache size; cache at most a certain
   * portion of the used FS, and at most a certain %age of remaining
   * memory
   */
  if((vm_info_stats(&vsi) != OK)) {
	bufs = 1024;
	printf("fslib: heuristic info fail: default to %d bufs\n", bufs);
	return bufs;
  }

  kbytes_remain_mem = div64u(mul64u(vsi.vsi_free, vsi.vsi_pagesize), 1024);

  /* check fs usage. */
  kbytes_used_fs = div64u(mul64u(bused, blocksize), 1024);
  kbytes_total_fs = div64u(mul64u(btotal, blocksize), 1024);

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

void
lmfs_markdirty(struct buf *bp)
{
	bp->lmfs_dirt = BP_DIRTY;
}

void
lmfs_markclean(struct buf *bp)
{
	bp->lmfs_dirt = BP_CLEAN;
}

int 
lmfs_isclean(struct buf *bp)
{
	return bp->lmfs_dirt == BP_CLEAN;
}

dev_t
lmfs_dev(struct buf *bp)
{
	return bp->lmfs_dev;
}

int lmfs_bytes(struct buf *bp)
{
	return bp->lmfs_bytes;
}

/*===========================================================================*
 *				lmfs_get_block				     *
 *===========================================================================*/
struct buf *lmfs_get_block(
  register dev_t dev,		/* on which device is the block? */
  register block_t block,	/* which block is wanted? */
  int only_search		/* if NO_READ, don't read, else act normal */
)
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
  static struct buf *bp, *prev_ptr;
  u64_t yieldid = VM_BLOCKID_NONE, getid = make64(dev, block);

  assert(buf_hash);
  assert(buf);
  assert(nr_bufs > 0);

  ASSERT(fs_block_size > 0);

  assert(dev != NO_DEV);

  /* Search the hash chain for (dev, block). Do_read() can use 
   * lmfs_get_block(NO_DEV ...) to get an unnamed block to fill with zeros when
   * someone wants to read from a hole in a file, in which case this search
   * is skipped
   */
  b = BUFHASH(block);
  bp = buf_hash[b];
  while (bp != NULL) {
  	if (bp->lmfs_blocknr == block && bp->lmfs_dev == dev) {
  		/* Block needed has been found. */
  		if (bp->lmfs_count == 0) rm_lru(bp);
  		bp->lmfs_count++;	/* record that block is in use */
  		ASSERT(bp->lmfs_bytes == fs_block_size);
  		ASSERT(bp->lmfs_dev == dev);
  		ASSERT(bp->lmfs_dev != NO_DEV);
  		ASSERT(bp->data);
  		return(bp);
  	} else {
  		/* This block is not the one sought. */
  		bp = bp->lmfs_hash; /* move to next block on hash chain */
  	}
  }

  /* Desired block is not on available chain.  Take oldest block ('front'). */
  if ((bp = front) == NULL) panic("all buffers in use: %d", nr_bufs);

  if(bp->lmfs_bytes < fs_block_size) {
	ASSERT(!bp->data);
	ASSERT(bp->lmfs_bytes == 0);
	if(!(bp->data = alloc_contig( (size_t) fs_block_size, 0, NULL))) {
		printf("fs cache: couldn't allocate a new block.\n");
		for(bp = front;
			bp && bp->lmfs_bytes < fs_block_size; bp = bp->lmfs_next)
			;
		if(!bp) {
			panic("no buffer available");
		}
	} else {
  		bp->lmfs_bytes = fs_block_size;
	}
  }

  ASSERT(bp);
  ASSERT(bp->data);
  ASSERT(bp->lmfs_bytes == fs_block_size);
  ASSERT(bp->lmfs_count == 0);

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

  /* If the block taken is dirty, make it clean by writing it to the disk.
   * Avoid hysteresis by flushing all other dirty blocks for the same device.
   */
  if (bp->lmfs_dev != NO_DEV) {
	if (bp->lmfs_dirt == BP_DIRTY) flushall(bp->lmfs_dev);

	/* Are we throwing out a block that contained something?
	 * Give it to VM for the second-layer cache.
	 */
	yieldid = make64(bp->lmfs_dev, bp->lmfs_blocknr);
	assert(bp->lmfs_bytes == fs_block_size);
	bp->lmfs_dev = NO_DEV;
  }

  /* Fill in block's parameters and add it to the hash chain where it goes. */
  MARKCLEAN(bp);		/* NO_DEV blocks may be marked dirty */
  bp->lmfs_dev = dev;		/* fill in device number */
  bp->lmfs_blocknr = block;	/* fill in block number */
  bp->lmfs_count++;		/* record that block is being used */
  b = BUFHASH(bp->lmfs_blocknr);
  bp->lmfs_hash = buf_hash[b];

  buf_hash[b] = bp;		/* add to hash list */

  assert(dev != NO_DEV);

  /* Go get the requested block unless searching or prefetching. */
  if(only_search == PREFETCH || only_search == NORMAL) {
	/* Block is not found in our cache, but we do want it
	 * if it's in the vm cache.
	 */
	if(vmcache) {
		/* If we can satisfy the PREFETCH or NORMAL request 
		 * from the vm cache, work is done.
		 */
		if(vm_yield_block_get_block(yieldid, getid,
			bp->data, fs_block_size) == OK) {
			return bp;
		}
	}
  }

  if(only_search == PREFETCH) {
	/* PREFETCH: don't do i/o. */
	bp->lmfs_dev = NO_DEV;
  } else if (only_search == NORMAL) {
	read_block(bp);
  } else if(only_search == NO_READ) {
	/* we want this block, but its contents
	 * will be overwritten. VM has to forget
	 * about it.
	 */
	if(vmcache) {
		vm_forgetblock(getid);
	}
  } else
	panic("unexpected only_search value: %d", only_search);

  assert(bp->data);

  return(bp);			/* return the newly acquired block */
}

/*===========================================================================*
 *				lmfs_put_block				     *
 *===========================================================================*/
void lmfs_put_block(bp, block_type)
register struct buf *bp;	/* pointer to the buffer to be released */
int block_type;			/* INODE_BLOCK, DIRECTORY_BLOCK, or whatever */
{
/* Return a block to the list of available blocks.   Depending on 'block_type'
 * it may be put on the front or rear of the LRU chain.  Blocks that are
 * expected to be needed again shortly (e.g., partially full data blocks)
 * go on the rear; blocks that are unlikely to be needed again shortly
 * (e.g., full data blocks) go on the front.  Blocks whose loss can hurt
 * the integrity of the file system (e.g., inode blocks) are written to
 * disk immediately if they are dirty.
 */
  if (bp == NULL) return;	/* it is easier to check here than in caller */

  bp->lmfs_count--;		/* there is one use fewer now */
  if (bp->lmfs_count != 0) return;	/* block is still in use */

  bufs_in_use--;		/* one fewer block buffers in use */

  /* Put this block back on the LRU chain.  */
  if (bp->lmfs_dev == DEV_RAM || (block_type & ONE_SHOT)) {
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
}

/*===========================================================================*
 *				read_block				     *
 *===========================================================================*/
static void read_block(bp)
register struct buf *bp;	/* buffer pointer */
{
/* Read or write a disk block. This is the only routine in which actual disk
 * I/O is invoked. If an error occurs, a message is printed here, but the error
 * is not reported to the caller.  If the error occurred while purging a block
 * from the cache, it is not clear what the caller could do about it anyway.
 */
  int r, op_failed;
  u64_t pos;
  dev_t dev = bp->lmfs_dev;

  op_failed = 0;

  assert(dev != NO_DEV);

  pos = mul64u(bp->lmfs_blocknr, fs_block_size);
  r = bdev_read(dev, pos, bp->data, fs_block_size,
  	BDEV_NOFLAGS);
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

  for (bp = &buf[0]; bp < &buf[nr_bufs]; bp++)
	if (bp->lmfs_dev == device) bp->lmfs_dev = NO_DEV;

  vm_forgetblocks();
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
       if (bp->lmfs_dirt == BP_DIRTY && bp->lmfs_dev == dev) {
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
  static iovec_t *iovec = NULL;
  u64_t pos;
  int j, r;

  STATICINIT(iovec, NR_IOREQS);

  /* (Shell) sort buffers on lmfs_blocknr. */
  gap = 1;
  do
	gap = 3 * gap + 1;
  while (gap <= bufqsize);
  while (gap != 1) {
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
	for (j = 0, iop = iovec; j < NR_IOREQS && j < bufqsize; j++, iop++) {
		bp = bufq[j];
		if (bp->lmfs_blocknr != (block_t) bufq[0]->lmfs_blocknr + j) break;
		iop->iov_addr = (vir_bytes) bp->data;
		iop->iov_size = (vir_bytes) fs_block_size;
	}
	pos = mul64u(bufq[0]->lmfs_blocknr, fs_block_size);
	if (rw_flag == READING)
		r = bdev_gather(dev, pos, iovec, j, BDEV_NOFLAGS);
	else
		r = bdev_scatter(dev, pos, iovec, j, BDEV_NOFLAGS);

	/* Harvest the results.  The driver may have returned an error, or it
	 * may have done less than what we asked for.
	 */
	if (r < 0) {
		printf("fs cache: I/O error %d on device %d/%d, block %u\n",
			r, major(dev), minor(dev), bufq[0]->lmfs_blocknr);
	}
	for (i = 0; i < j; i++) {
		bp = bufq[i];
		if (r < (ssize_t) fs_block_size) {
			/* Transfer failed. */
			if (i == 0) {
				bp->lmfs_dev = NO_DEV;	/* Invalidate block */
				vm_forgetblocks();
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
}

/*===========================================================================*
 *				rm_lru					     *
 *===========================================================================*/
static void rm_lru(bp)
struct buf *bp;
{
/* Remove a block from its LRU chain. */
  struct buf *next_ptr, *prev_ptr;

  bufs_in_use++;
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

/*===========================================================================*
 *			lmfs_set_blocksize				     *
 *===========================================================================*/
void lmfs_set_blocksize(int new_block_size, int major)
{
  int bufs;
  u32_t btotal, bfree, bused;

  cache_resize(new_block_size, MINBUFS);

  fs_blockstats(&btotal, &bfree, &bused);

  bufs = fs_bufs_heuristic(10, btotal, bfree,
        new_block_size, major);

  cache_resize(new_block_size, bufs);
  
  /* Decide whether to use seconday cache or not.
   * Only do this if
   *	- it's available, and
   *	- use of it hasn't been disabled for this fs, and
   *	- our main FS device isn't a memory device
   */

  vmcache = 0;
  if(vm_forgetblock(VM_BLOCKID_NONE) != ENOSYS &&
  	may_use_vmcache && major != MEMORY_MAJOR) {
	vmcache = 1;
  }
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
			free_contig(bp->data, bp->lmfs_bytes);
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

  vm_forgetblocks();
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
		if(bp->lmfs_dev != NO_DEV && bp->lmfs_dirt == BP_DIRTY) 
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
