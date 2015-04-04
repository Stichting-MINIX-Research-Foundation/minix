#include "fs.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "buf.h"
#include "inode.h"
#include "super.h"
#include <sys/param.h>
#include <sys/dirent.h>
#include <assert.h>


static struct buf *rahead(struct inode *rip, block_t baseblock, u64_t
	position, unsigned bytes_ahead);
static int rw_chunk(struct inode *rip, u64_t position, unsigned off,
	size_t chunk, unsigned left, int call, struct fsdriver_data *data,
	unsigned buf_off, unsigned int block_size, int *completed);


/*===========================================================================*
 *				fs_readwrite				     *
 *===========================================================================*/
ssize_t fs_readwrite(ino_t ino_nr, struct fsdriver_data *data, size_t nrbytes,
	off_t position, int call)
{
  int r;
  int regular;
  off_t f_size, bytes_left;
  size_t off, cum_io, block_size, chunk;
  mode_t mode_word;
  int completed;
  struct inode *rip;
  
  r = OK;
  
  /* Find the inode referred */
  if ((rip = find_inode(fs_dev, ino_nr)) == NULL)
	return(EINVAL);

  mode_word = rip->i_mode & I_TYPE;
  regular = (mode_word == I_REGULAR);
  
  /* Determine blocksize */
  block_size = rip->i_sp->s_block_size;
  f_size = rip->i_size;

  /* If this is file i/o, check we can write */
  if (call == FSC_WRITE) {
  	  if(rip->i_sp->s_rd_only) 
		  return EROFS;

	  /* Check in advance to see if file will grow too big. */
	  if (position > (off_t) (rip->i_sp->s_max_size - nrbytes))
		  return(EFBIG);

	  /* Clear the zone containing present EOF if hole about
	   * to be created.  This is necessary because all unwritten
	   * blocks prior to the EOF must read as zeros.
	   */
	  if(position > f_size) clear_zone(rip, f_size, 0);
  }

  cum_io = 0;
  /* Split the transfer into chunks that don't span two blocks. */
  while (nrbytes > 0) {
	  off = ((unsigned int) position) % block_size; /* offset in blk*/
	  chunk = block_size - off;
	  if (chunk > nrbytes)
		chunk = nrbytes;

	  if (call != FSC_WRITE) {
		  bytes_left = f_size - position;
		  if (position >= f_size) break;	/* we are beyond EOF */
		  if (chunk > (unsigned int) bytes_left) chunk = bytes_left;
	  }
	  
	  /* Read or write 'chunk' bytes. */
	  r = rw_chunk(rip, ((u64_t)((unsigned long)position)), off, chunk,
		nrbytes, call, data, cum_io, block_size, &completed);

	  if (r != OK) break;

	  /* Update counters and pointers. */
	  nrbytes -= chunk;	/* bytes yet to be read */
	  cum_io += chunk;	/* bytes read so far */
	  position += (off_t) chunk;	/* position within the file */
  }

  /* On write, update file size and access time. */
  if (call == FSC_WRITE) {
	  if (regular || mode_word == I_DIRECTORY) {
		  if (position > f_size) rip->i_size = position;
	  }
  } 

  rip->i_seek = NO_SEEK;

  if (r != OK)
	return r;

  /* even on a ROFS, writing to a device node on it is fine, 
   * just don't update the inode stats for it. And dito for reading.
   */
  if (!rip->i_sp->s_rd_only) {
	  if (call == FSC_READ) rip->i_update |= ATIME;
	  if (call == FSC_WRITE) rip->i_update |= CTIME | MTIME;
	  IN_MARKDIRTY(rip);		/* inode is thus now dirty */
  }
  
  return cum_io;
}


/*===========================================================================*
 *				rw_chunk				     *
 *===========================================================================*/
static int rw_chunk(rip, position, off, chunk, left, call, data, buf_off,
	block_size, completed)
register struct inode *rip;	/* pointer to inode for file to be rd/wr */
u64_t position;			/* position within file to read or write */
unsigned off;			/* off within the current block */
size_t chunk;			/* number of bytes to read or write */
unsigned left;			/* max number of bytes wanted after position */
int call;			/* FSC_READ, FSC_WRITE, or FSC_PEEK */
struct fsdriver_data *data;	/* structure for (remote) user buffer */
unsigned buf_off;		/* offset in user buffer */
unsigned int block_size;	/* block size of FS operating on */
int *completed;			/* number of bytes copied */
{
/* Read or write (part of) a block. */
  struct buf *bp = NULL;
  register int r = OK;
  int n;
  block_t b;
  dev_t dev;
  ino_t ino = VMC_NO_INODE;
  u64_t ino_off = rounddown(position, block_size);

  *completed = 0;

  if (ex64hi(position) != 0)
	panic("rw_chunk: position too high");
  b = read_map(rip, (off_t) ex64lo(position), 0);
  dev = rip->i_dev;
  ino = rip->i_num;
  assert(ino != VMC_NO_INODE);

  if (b == NO_BLOCK) {
	if (call == FSC_READ) {
		/* Reading from a nonexistent block.  Must read as all zeros.*/
		r = fsdriver_zero(data, buf_off, chunk);
		if(r != OK) {
			printf("MFS: fsdriver_zero failed\n");
		}
		return r;
	} else if (call == FSC_PEEK) {
		/* Peeking a nonexistent block. Report to VM. */
		lmfs_zero_block_ino(dev, ino, ino_off);
		return OK;
	} else {
		/* Writing to a nonexistent block.
		 * Create and enter in inode.
		 */
		if ((bp = new_block(rip, (off_t) ex64lo(position))) == NULL)
			return(err_code);
	}
  } else if (call != FSC_WRITE) {
	/* Read and read ahead if convenient. */
	bp = rahead(rip, b, position, left);
  } else {
	/* Normally an existing block to be partially overwritten is first read
	 * in.  However, a full block need not be read in.  If it is already in
	 * the cache, acquire it, otherwise just acquire a free buffer.
	 */
	n = (chunk == block_size ? NO_READ : NORMAL);
	if (off == 0 && (off_t) ex64lo(position) >= rip->i_size)
		n = NO_READ;
	assert(ino != VMC_NO_INODE);
	assert(!(ino_off % block_size));
	if ((r = lmfs_get_block_ino(&bp, dev, b, n, ino, ino_off)) != OK)
		panic("MFS: error getting block (%llu,%u): %d", dev, b, r);
  }

  /* In all cases, bp now points to a valid buffer. */
  assert(bp != NULL);
  
  if (call == FSC_WRITE && chunk != block_size &&
      (off_t) ex64lo(position) >= rip->i_size && off == 0) {
	zero_block(bp);
  }

  if (call == FSC_READ) {
	/* Copy a chunk from the block buffer to user space. */
	r = fsdriver_copyout(data, buf_off, b_data(bp)+off, chunk);
  } else if (call == FSC_WRITE) {
	/* Copy a chunk from user space to the block buffer. */
	r = fsdriver_copyin(data, buf_off, b_data(bp)+off, chunk);
	MARKDIRTY(bp);
  }
  
  put_block(bp);

  return(r);
}


/*===========================================================================*
 *				read_map				     *
 *===========================================================================*/
block_t read_map(rip, position, opportunistic)
register struct inode *rip;	/* ptr to inode to map from */
off_t position;			/* position in file whose blk wanted */
int opportunistic;		/* if nonzero, only use cache for metadata */
{
/* Given an inode and a position within the corresponding file, locate the
 * block (not zone) number in which that position is to be found and return it.
 */

  struct buf *bp;
  zone_t z;
  int scale, boff, index, zind;
  unsigned int dzones, nr_indirects;
  block_t b;
  unsigned long excess, zone, block_pos;
  int iomode;

  iomode = opportunistic ? PEEK : NORMAL;

  scale = rip->i_sp->s_log_zone_size;	/* for block-zone conversion */
  block_pos = position/rip->i_sp->s_block_size;	/* relative blk # in file */
  zone = block_pos >> scale;	/* position's zone */
  boff = (int) (block_pos - (zone << scale) ); /* relative blk # within zone */
  dzones = rip->i_ndzones;
  nr_indirects = rip->i_nindirs;

  /* Is 'position' to be found in the inode itself? */
  if (zone < dzones) {
	zind = (int) zone;	/* index should be an int */
	z = rip->i_zone[zind];
	if (z == NO_ZONE) return(NO_BLOCK);
	b = (block_t) ((z << scale) + boff);
	return(b);
  }

  /* It is not in the inode, so it must be single or double indirect. */
  excess = zone - dzones;	/* first Vx_NR_DZONES don't count */

  if (excess < nr_indirects) {
	/* 'position' can be located via the single indirect block. */
	z = rip->i_zone[dzones];
  } else {
	/* 'position' can be located via the double indirect block. */
	if ( (z = rip->i_zone[dzones+1]) == NO_ZONE) return(NO_BLOCK);
	excess -= nr_indirects;			/* single indir doesn't count*/
	b = (block_t) z << scale;
	ASSERT(rip->i_dev != NO_DEV);
	index = (int) (excess/nr_indirects);
	if ((unsigned int) index > rip->i_nindirs)
		return(NO_BLOCK);	/* Can't go beyond double indirects */
	bp = get_block(rip->i_dev, b, iomode); /* get double indirect block */
	if (bp == NULL)
		return NO_BLOCK;		/* peeking failed */
	z = rd_indir(bp, index);		/* z= zone for single*/
	put_block(bp);				/* release double ind block */
	excess = excess % nr_indirects;		/* index into single ind blk */
  }

  /* 'z' is zone num for single indirect block; 'excess' is index into it. */
  if (z == NO_ZONE) return(NO_BLOCK);
  b = (block_t) z << scale;			/* b is blk # for single ind */
  bp = get_block(rip->i_dev, b, iomode);	/* get single indirect block */
  if (bp == NULL)
	return NO_BLOCK;			/* peeking failed */
  z = rd_indir(bp, (int) excess);		/* get block pointed to */
  put_block(bp);				/* release single indir blk */
  if (z == NO_ZONE) return(NO_BLOCK);
  b = (block_t) ((z << scale) + boff);
  return(b);
}

struct buf *get_block_map(register struct inode *rip, u64_t position)
{
	struct buf *bp;
	int r, block_size;
	block_t b = read_map(rip, position, 0);	/* get block number */
	if(b == NO_BLOCK)
		return NULL;
	block_size = get_block_size(rip->i_dev);
	position = rounddown(position, block_size);
	assert(rip->i_num != VMC_NO_INODE);
	if ((r = lmfs_get_block_ino(&bp, rip->i_dev, b, NORMAL, rip->i_num,
	    position)) != OK)
		panic("MFS: error getting block (%llu,%u): %d",
		    rip->i_dev, b, r);
	return bp;
}

/*===========================================================================*
 *				rd_indir				     *
 *===========================================================================*/
zone_t rd_indir(bp, index)
struct buf *bp;			/* pointer to indirect block */
int index;			/* index into *bp */
{
  struct super_block *sp;
  zone_t zone;

  if(bp == NULL)
	panic("rd_indir() on NULL");

  sp = &superblock;

  /* read a zone from an indirect block */
  assert(sp->s_version == V3);
  zone = (zone_t) conv4(sp->s_native, (long) b_v2_ind(bp)[index]);

  if (zone != NO_ZONE &&
		(zone < (zone_t) sp->s_firstdatazone || zone >= sp->s_zones)) {
	printf("Illegal zone number %ld in indirect block, index %d\n",
	       (long) zone, index);
	panic("check file system");
  }
  
  return(zone);
}

/*===========================================================================*
 *				rahead					     *
 *===========================================================================*/
static struct buf *rahead(rip, baseblock, position, bytes_ahead)
register struct inode *rip;	/* pointer to inode for file to be read */
block_t baseblock;		/* block at current position */
u64_t position;			/* position within file */
unsigned bytes_ahead;		/* bytes beyond position for immediate use */
{
/* Fetch a block from the cache or the device.  If a physical read is
 * required, prefetch as many more blocks as convenient into the cache.
 * This usually covers bytes_ahead and is at least BLOCKS_MINIMUM.
 * The device driver may decide it knows better and stop reading at a
 * cylinder boundary (or after an error).  Rw_scattered() puts an optional
 * flag on all reads to allow this.
 */
/* Minimum number of blocks to prefetch. */
# define BLOCKS_MINIMUM		32
  int r, scale, read_q_size;
  unsigned int blocks_ahead, fragment, block_size;
  block_t block, blocks_left;
  off_t ind1_pos;
  dev_t dev;
  struct buf *bp;
  static block64_t read_q[LMFS_MAX_PREFETCH];
  u64_t position_running;

  dev = rip->i_dev;
  assert(dev != NO_DEV);
  
  block_size = get_block_size(dev);

  block = baseblock;

  fragment = position % block_size;
  position -= fragment;
  position_running = position;
  bytes_ahead += fragment;
  blocks_ahead = (bytes_ahead + block_size - 1) / block_size;

  r = lmfs_get_block_ino(&bp, dev, block, PEEK, rip->i_num, position);
  if (r == OK)
	return(bp);
  if (r != ENOENT)
	panic("MFS: error getting block (%llu,%u): %d", dev, block, r);

  /* The best guess for the number of blocks to prefetch:  A lot.
   * It is impossible to tell what the device looks like, so we don't even
   * try to guess the geometry, but leave it to the driver.
   *
   * The floppy driver can read a full track with no rotational delay, and it
   * avoids reading partial tracks if it can, so handing it enough buffers to
   * read two tracks is perfect.  (Two, because some diskette types have
   * an odd number of sectors per track, so a block may span tracks.)
   *
   * The disk drivers don't try to be smart.  With todays disks it is
   * impossible to tell what the real geometry looks like, so it is best to
   * read as much as you can.  With luck the caching on the drive allows
   * for a little time to start the next read.
   *
   * The current solution below is a bit of a hack, it just reads blocks from
   * the current file position hoping that more of the file can be found.  A
   * better solution must look at the already available zone pointers and
   * indirect blocks (but don't call read_map!).
   */

  blocks_left = (block_t) (rip->i_size-ex64lo(position)+(block_size-1)) /
								block_size;

  /* Go for the first indirect block if we are in its neighborhood. */
  scale = rip->i_sp->s_log_zone_size;
  ind1_pos = (off_t) rip->i_ndzones * (block_size << scale);
  if ((off_t) ex64lo(position) <= ind1_pos && rip->i_size > ind1_pos) {
	blocks_ahead++;
	blocks_left++;
  }

  /* Read at least the minimum number of blocks, but not after a seek. */
  if (blocks_ahead < BLOCKS_MINIMUM && rip->i_seek == NO_SEEK)
	blocks_ahead = BLOCKS_MINIMUM;

  /* Can't go past end of file. */
  if (blocks_ahead > blocks_left) blocks_ahead = blocks_left;

  /* No more than the maximum request. */
  if (blocks_ahead > LMFS_MAX_PREFETCH) blocks_ahead = LMFS_MAX_PREFETCH;

  read_q_size = 0;

  /* Acquire block buffers. */
  for (;;) {
  	block_t thisblock;
	read_q[read_q_size++] = block;

	if (--blocks_ahead == 0) break;

	block++;
	position_running += block_size;

	thisblock = read_map(rip, (off_t) ex64lo(position_running), 1);
	if (thisblock != NO_BLOCK) {
		r = lmfs_get_block_ino(&bp, dev, thisblock, PEEK, rip->i_num,
		    position_running);
		block = thisblock;
	} else
		r = lmfs_get_block(&bp, dev, block, PEEK);

	if (r == OK) {
		/* Oops, block already in the cache, get out. */
		put_block(bp);
		break;
	}
	if (r != ENOENT)
		panic("MFS: error getting block (%llu,%u): %d", dev, block, r);
  }
  lmfs_prefetch(dev, read_q, read_q_size);

  r = lmfs_get_block_ino(&bp, dev, baseblock, NORMAL, rip->i_num, position);
  if (r != OK)
	panic("MFS: error getting block (%llu,%u): %d", dev, baseblock, r);
  return bp;
}


/*===========================================================================*
 *				fs_getdents				     *
 *===========================================================================*/
ssize_t fs_getdents(ino_t ino_nr, struct fsdriver_data *data, size_t bytes,
	off_t *posp)
{
#define GETDENTS_BUFSIZE	(sizeof(struct dirent) + MFS_NAME_MAX + 1)
#define GETDENTS_ENTRIES	8
  static char getdents_buf[GETDENTS_BUFSIZE * GETDENTS_ENTRIES];
  struct fsdriver_dentry fsdentry;
  struct inode *rip, *entrip;
  int r, done;
  unsigned int block_size, len, type;
  off_t pos, off, block_pos, new_pos, ent_pos;
  struct buf *bp;
  struct direct *dp;
  char *cp;

  /* Check whether the position is properly aligned */
  pos = *posp;
  if( (unsigned int) pos % DIR_ENTRY_SIZE)
	  return(ENOENT);
  
  if( (rip = get_inode(fs_dev, ino_nr)) == NULL)
	  return(EINVAL);

  block_size = rip->i_sp->s_block_size;
  off = (pos % block_size);		/* Offset in block */
  block_pos = pos - off;
  done = FALSE;		/* Stop processing directory blocks when done is set */

  fsdriver_dentry_init(&fsdentry, data, bytes, getdents_buf,
	sizeof(getdents_buf));

  /* The default position for the next request is EOF. If the user's buffer
   * fills up before EOF, new_pos will be modified. */
  new_pos = rip->i_size;

  r = 0;

  for(; block_pos < rip->i_size; block_pos += block_size) {
	/* Since directories don't have holes, 'bp' cannot be NULL. */
	bp = get_block_map(rip, block_pos);	/* get a dir block */
	assert(bp != NULL);

	/* Search a directory block. */
	if (block_pos < pos)
		dp = &b_dir(bp)[off / DIR_ENTRY_SIZE];
	else
		dp = &b_dir(bp)[0];
	for (; dp < &b_dir(bp)[NR_DIR_ENTRIES(block_size)]; dp++) {
		if (dp->mfs_d_ino == 0)
			continue;	/* Entry is not in use */

		/* Compute the length of the name */
		cp = memchr(dp->mfs_d_name, '\0', sizeof(dp->mfs_d_name));
		if (cp == NULL)
			len = sizeof(dp->mfs_d_name);
		else
			len = cp - (dp->mfs_d_name);

		/* Need the position of this entry in the directory */
		ent_pos = block_pos + ((char *) dp - (char *) bp->data);

		/* We also need(?) the file type of the target inode. */
		if (!(entrip = get_inode(fs_dev, (ino_t) dp->mfs_d_ino)))
			panic("unexpected get_inode failure");
		type = IFTODT(entrip->i_mode);
		put_inode(entrip);

		/* MFS does not store file types in its directory entries, and
		 * fetching the mode from the inode is seriously expensive.
		 * Userland should always be prepared to receive DT_UNKNOWN.
		 */
		r = fsdriver_dentry_add(&fsdentry, (ino_t) dp->mfs_d_ino,
			dp->mfs_d_name, len, type);

		/* If the user buffer is full, or an error occurred, stop. */
		if (r <= 0) {
			done = TRUE;

			/* Record the position of this entry, it is the
			 * starting point of the next request (unless the
			 * postion is modified with lseek).
			 */
			new_pos = ent_pos;
			break;
		}
	}

	put_block(bp);
	if (done)
		break;
  }

  if (r >= 0 && (r = fsdriver_dentry_finish(&fsdentry)) >= 0) {
	  *posp = new_pos;
	  if(!rip->i_sp->s_rd_only) {
		  rip->i_update |= ATIME;
		  IN_MARKDIRTY(rip);
	  }
  }

  put_inode(rip);		/* release the inode */
  return(r);
}
