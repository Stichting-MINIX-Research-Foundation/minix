#include "fs.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <minix/com.h>
#include <minix/u64.h>
#include "buf.h"
#include "inode.h"
#include "super.h"
#include <minix/vfsif.h>
#include <sys/param.h>
#include <assert.h>


static struct buf *rahead(struct inode *rip, block_t baseblock, u64_t
	position, unsigned bytes_ahead);
static int rw_chunk(struct inode *rip, u64_t position, unsigned off,
	size_t chunk, unsigned left, int rw_flag, cp_grant_id_t gid, unsigned
	buf_off, unsigned int block_size, int *completed);


/*===========================================================================*
 *				fs_readwrite				     *
 *===========================================================================*/
int fs_readwrite(void)
{
  int r, rw_flag, block_spec;
  int regular;
  cp_grant_id_t gid;
  off_t position, f_size, bytes_left;
  unsigned int off, cum_io, block_size, chunk;
  mode_t mode_word;
  int completed;
  struct inode *rip;
  size_t nrbytes;
  
  r = OK;
  
  /* Find the inode referred */
  if ((rip = find_inode(fs_dev, (ino_t) fs_m_in.REQ_INODE_NR)) == NULL)
	return(EINVAL);

  mode_word = rip->i_mode & I_TYPE;
  regular = (mode_word == I_REGULAR || mode_word == I_NAMED_PIPE);
  block_spec = (mode_word == I_BLOCK_SPECIAL ? 1 : 0);
  
  /* Determine blocksize */
  if (block_spec) {
	block_size = get_block_size( (dev_t) rip->i_zone[0]);
	f_size = MAX_FILE_POS;
  } else {
  	block_size = rip->i_sp->s_block_size;
  	f_size = rip->i_size;
  }

  /* Get the values from the request message */ 
  switch(fs_m_in.m_type) {
  	case REQ_READ: rw_flag = READING; break;
  	case REQ_WRITE: rw_flag = WRITING; break;
  	case REQ_PEEK: rw_flag = PEEKING; break;
	default: panic("odd request");
  }
  gid = (cp_grant_id_t) fs_m_in.REQ_GRANT;
  position = (off_t) fs_m_in.REQ_SEEK_POS_LO;
  nrbytes = (size_t) fs_m_in.REQ_NBYTES;

  lmfs_reset_rdwt_err();

  /* If this is file i/o, check we can write */
  if (rw_flag == WRITING && !block_spec) {
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

  /* If this is block i/o, check we can write */
  if(block_spec && rw_flag == WRITING &&
  	(dev_t) rip->i_zone[0] == superblock.s_dev && superblock.s_rd_only)
		return EROFS;
	      
  cum_io = 0;
  /* Split the transfer into chunks that don't span two blocks. */
  while (nrbytes > 0) {
	  off = ((unsigned int) position) % block_size; /* offset in blk*/
	  chunk = min(nrbytes, block_size - off);

	  if (rw_flag == READING) {
		  bytes_left = f_size - position;
		  if (position >= f_size) break;	/* we are beyond EOF */
		  if (chunk > (unsigned int) bytes_left) chunk = bytes_left;
	  }
	  
	  /* Read or write 'chunk' bytes. */
	  r = rw_chunk(rip, cvul64((unsigned long) position), off, chunk,
	  	       nrbytes, rw_flag, gid, cum_io, block_size, &completed);

	  if (r != OK) break;	/* EOF reached */
	  if (lmfs_rdwt_err() < 0) break;

	  /* Update counters and pointers. */
	  nrbytes -= chunk;	/* bytes yet to be read */
	  cum_io += chunk;	/* bytes read so far */
	  position += (off_t) chunk;	/* position within the file */
  }

  fs_m_out.RES_SEEK_POS_LO = position; /* It might change later and the VFS
					   has to know this value */
  
  /* On write, update file size and access time. */
  if (rw_flag == WRITING) {
	  if (regular || mode_word == I_DIRECTORY) {
		  if (position > f_size) rip->i_size = position;
	  }
  } 

  rip->i_seek = NO_SEEK;

  if (lmfs_rdwt_err() != OK) r = lmfs_rdwt_err();	/* check for disk error */
  if (lmfs_rdwt_err() == END_OF_FILE) r = OK;

  /* even on a ROFS, writing to a device node on it is fine, 
   * just don't update the inode stats for it. And dito for reading.
   */
  if (r == OK && !rip->i_sp->s_rd_only) {
	  if (rw_flag == READING) rip->i_update |= ATIME;
	  if (rw_flag == WRITING) rip->i_update |= CTIME | MTIME;
	  IN_MARKDIRTY(rip);		/* inode is thus now dirty */
  }
  
  fs_m_out.RES_NBYTES = cum_io;
  
  return(r);
}


/*===========================================================================*
 *				fs_breadwrite				     *
 *===========================================================================*/
int fs_breadwrite(void)
{
  int r, rw_flag, completed;
  cp_grant_id_t gid;
  u64_t position;
  unsigned int off, cum_io, chunk, block_size;
  size_t nrbytes;
  dev_t target_dev;

  /* Pseudo inode for rw_chunk */
  struct inode rip;
  
  r = OK;

  target_dev = (dev_t) fs_m_in.REQ_DEV2;
  
  /* Get the values from the request message */ 
  rw_flag = (fs_m_in.m_type == REQ_BREAD ? READING : WRITING);
  gid = (cp_grant_id_t) fs_m_in.REQ_GRANT;
  position = make64((unsigned long) fs_m_in.REQ_SEEK_POS_LO,
  		    (unsigned long) fs_m_in.REQ_SEEK_POS_HI);
  nrbytes = (size_t) fs_m_in.REQ_NBYTES;
  
  block_size = get_block_size(target_dev);

  /* Don't block-write to a RO-mounted filesystem. */
  if(superblock.s_dev == target_dev && superblock.s_rd_only)
  	return EROFS;

  rip.i_zone[0] = (zone_t) target_dev;
  rip.i_mode = I_BLOCK_SPECIAL;
  rip.i_size = 0;

  lmfs_reset_rdwt_err();
  
  cum_io = 0;
  /* Split the transfer into chunks that don't span two blocks. */
  while (nrbytes > 0) {
	  off = rem64u(position, block_size);	/* offset in blk*/
	  chunk = min(nrbytes, block_size - off);

	  /* Read or write 'chunk' bytes. */
	  r = rw_chunk(&rip, position, off, chunk, nrbytes, rw_flag, gid,
	  	       cum_io, block_size, &completed);

	  if (r != OK) break;	/* EOF reached */
	  if (lmfs_rdwt_err() < 0) break;

	  /* Update counters and pointers. */
	  nrbytes -= chunk;	        /* bytes yet to be read */
	  cum_io += chunk;	        /* bytes read so far */
	  position = add64ul(position, chunk);	/* position within the file */
  }
  
  fs_m_out.RES_SEEK_POS_LO = ex64lo(position); 
  fs_m_out.RES_SEEK_POS_HI = ex64hi(position); 
  
  if (lmfs_rdwt_err() != OK) r = lmfs_rdwt_err();	/* check for disk error */
  if (lmfs_rdwt_err() == END_OF_FILE) r = OK;

  fs_m_out.RES_NBYTES = cum_io;
  
  return(r);
}


/*===========================================================================*
 *				rw_chunk				     *
 *===========================================================================*/
static int rw_chunk(rip, position, off, chunk, left, rw_flag, gid,
 buf_off, block_size, completed)
register struct inode *rip;	/* pointer to inode for file to be rd/wr */
u64_t position;			/* position within file to read or write */
unsigned off;			/* off within the current block */
unsigned int chunk;		/* number of bytes to read or write */
unsigned left;			/* max number of bytes wanted after position */
int rw_flag;			/* READING, WRITING or PEEKING */
cp_grant_id_t gid;		/* grant */
unsigned buf_off;		/* offset in grant */
unsigned int block_size;	/* block size of FS operating on */
int *completed;			/* number of bytes copied */
{
/* Read or write (part of) a block. */

  register struct buf *bp = NULL;
  register int r = OK;
  int n, block_spec;
  block_t b;
  dev_t dev;
  ino_t ino = VMC_NO_INODE;
  u64_t ino_off = rounddown(position, block_size);

  /* rw_flag:
   *   READING: read from FS, copy to user
   *   WRITING: copy from user, write to FS
   *   PEEKING: try to get all the blocks into the cache, no copying
   */

  *completed = 0;

  block_spec = (rip->i_mode & I_TYPE) == I_BLOCK_SPECIAL;

  if (block_spec) {
	b = div64u(position, block_size);
	dev = (dev_t) rip->i_zone[0];
  } else {
	if (ex64hi(position) != 0)
		panic("rw_chunk: position too high");
	b = read_map(rip, (off_t) ex64lo(position), 0);
	dev = rip->i_dev;
	ino = rip->i_num;
	assert(ino != VMC_NO_INODE);
  }

  if (!block_spec && b == NO_BLOCK) {
	if (rw_flag == READING) {
		/* Reading from a nonexistent block.  Must read as all zeros.*/
		r = sys_safememset(VFS_PROC_NR, gid, (vir_bytes) buf_off,
			   0, (size_t) chunk);
		if(r != OK) {
			printf("MFS: sys_safememset failed\n");
		}
		return r;
	} else {
		/* Writing to or peeking a nonexistent block.
		 * Create and enter in inode.
		 */
		if ((bp = new_block(rip, (off_t) ex64lo(position))) == NULL)
			return(err_code);
	}
  } else if (rw_flag == READING || rw_flag == PEEKING) {
	/* Read and read ahead if convenient. */
	bp = rahead(rip, b, position, left);
  } else {
	/* Normally an existing block to be partially overwritten is first read
	 * in.  However, a full block need not be read in.  If it is already in
	 * the cache, acquire it, otherwise just acquire a free buffer.
	 */
	n = (chunk == block_size ? NO_READ : NORMAL);
	if (!block_spec && off == 0 && (off_t) ex64lo(position) >= rip->i_size) 
		n = NO_READ;
	if(block_spec) {
		assert(ino == VMC_NO_INODE);
		bp = get_block(dev, b, n);
	} else {
		assert(ino != VMC_NO_INODE);
		assert(!(ino_off % block_size));
		bp = lmfs_get_block_ino(dev, b, n, ino, ino_off);
	}
  }

  /* In all cases, bp now points to a valid buffer. */
  assert(bp);
  
  if (rw_flag == WRITING && chunk != block_size && !block_spec &&
      (off_t) ex64lo(position) >= rip->i_size && off == 0) {
	zero_block(bp);
  }

  if (rw_flag == READING) {
	/* Copy a chunk from the block buffer to user space. */
	r = sys_safecopyto(VFS_PROC_NR, gid, (vir_bytes) buf_off,
			   (vir_bytes) (b_data(bp)+off), (size_t) chunk);
  } else if(rw_flag == WRITING) {
	/* Copy a chunk from user space to the block buffer. */
	r = sys_safecopyfrom(VFS_PROC_NR, gid, (vir_bytes) buf_off,
			     (vir_bytes) (b_data(bp)+off), (size_t) chunk);
	MARKDIRTY(bp);
  }
  
  n = (off + chunk == block_size ? FULL_DATA_BLOCK : PARTIAL_DATA_BLOCK);
  put_block(bp, n);

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
  int iomode = NORMAL;

  if(opportunistic) iomode = PREFETCH;

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
	if(opportunistic && lmfs_dev(bp) == NO_DEV) {
		put_block(bp, INDIRECT_BLOCK);
		return NO_BLOCK;
	}
	ASSERT(lmfs_dev(bp) != NO_DEV);
	ASSERT(lmfs_dev(bp) == rip->i_dev);
	z = rd_indir(bp, index);		/* z= zone for single*/
	put_block(bp, INDIRECT_BLOCK);		/* release double ind block */
	excess = excess % nr_indirects;		/* index into single ind blk */
  }

  /* 'z' is zone num for single indirect block; 'excess' is index into it. */
  if (z == NO_ZONE) return(NO_BLOCK);
  b = (block_t) z << scale;			/* b is blk # for single ind */
  bp = get_block(rip->i_dev, b, iomode);	/* get single indirect block */
  if(opportunistic && lmfs_dev(bp) == NO_DEV) {
	put_block(bp, INDIRECT_BLOCK);
	return NO_BLOCK;
  }
  z = rd_indir(bp, (int) excess);		/* get block pointed to */
  put_block(bp, INDIRECT_BLOCK);		/* release single indir blk */
  if (z == NO_ZONE) return(NO_BLOCK);
  b = (block_t) ((z << scale) + boff);
  return(b);
}

struct buf *get_block_map(register struct inode *rip, u64_t position)
{
	block_t b = read_map(rip, position, 0);	/* get block number */
	int block_size = get_block_size(rip->i_dev);
	if(b == NO_BLOCK)
		return NULL;
	position = rounddown(position, block_size);
	assert(rip->i_num != VMC_NO_INODE);
	return lmfs_get_block_ino(rip->i_dev, b, NORMAL, rip->i_num, position);
}

/*===========================================================================*
 *				rd_indir				     *
 *===========================================================================*/
zone_t rd_indir(bp, index)
struct buf *bp;			/* pointer to indirect block */
int index;			/* index into *bp */
{
/* Given a pointer to an indirect block, read one entry.  The reason for
 * making a separate routine out of this is that there are four cases:
 * V1 (IBM and 68000), and V2 (IBM and 68000).
 */

  struct super_block *sp;
  zone_t zone;			/* V2 zones are longs (shorts in V1) */

  if(bp == NULL)
	panic("rd_indir() on NULL");

  sp = get_super(lmfs_dev(bp));	/* need super block to find file sys type */

  /* read a zone from an indirect block */
  if (sp->s_version == V1)
	zone = (zone_t) conv2(sp->s_native, (int)  b_v1_ind(bp)[index]);
  else
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
  int nr_bufs = lmfs_nr_bufs();
# define BLOCKS_MINIMUM		(nr_bufs < 50 ? 18 : 32)
  int block_spec, scale, read_q_size;
  unsigned int blocks_ahead, fragment, block_size;
  block_t block, blocks_left;
  off_t ind1_pos;
  dev_t dev;
  struct buf *bp;
  static unsigned int readqsize = 0;
  static struct buf **read_q;
  u64_t position_running;

  if(readqsize != nr_bufs) {
	if(readqsize > 0) {
		assert(read_q != NULL);
		free(read_q);
	}
	if(!(read_q = malloc(sizeof(read_q[0])*nr_bufs)))
		panic("couldn't allocate read_q");
	readqsize = nr_bufs;
  }

  block_spec = (rip->i_mode & I_TYPE) == I_BLOCK_SPECIAL;
  if (block_spec) 
	dev = (dev_t) rip->i_zone[0];
  else 
	dev = rip->i_dev;

  assert(dev != NO_DEV);
  
  block_size = get_block_size(dev);

  block = baseblock;

  fragment = position % block_size;
  position -= fragment;
  position_running = position;
  bytes_ahead += fragment;
  blocks_ahead = (bytes_ahead + block_size - 1) / block_size;

  if(block_spec)
	  bp = get_block(dev, block, PREFETCH);
  else
	  bp = lmfs_get_block_ino(dev, block, PREFETCH, rip->i_num, position);

  assert(bp != NULL);
  if (lmfs_dev(bp) != NO_DEV) return(bp);

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

  if (block_spec && rip->i_size == 0) {
	blocks_left = (block_t) NR_IOREQS;
  } else {
	blocks_left = (block_t) (rip->i_size-ex64lo(position)+(block_size-1)) /
								block_size;

	/* Go for the first indirect block if we are in its neighborhood. */
	if (!block_spec) {
		scale = rip->i_sp->s_log_zone_size;
		ind1_pos = (off_t) rip->i_ndzones * (block_size << scale);
		if ((off_t) ex64lo(position) <= ind1_pos &&
		     rip->i_size > ind1_pos) {
			blocks_ahead++;
			blocks_left++;
		}
	}
  }

  /* No more than the maximum request. */
  if (blocks_ahead > NR_IOREQS) blocks_ahead = NR_IOREQS;

  /* Read at least the minimum number of blocks, but not after a seek. */
  if (blocks_ahead < BLOCKS_MINIMUM && rip->i_seek == NO_SEEK)
	blocks_ahead = BLOCKS_MINIMUM;

  /* Can't go past end of file. */
  if (blocks_ahead > blocks_left) blocks_ahead = blocks_left;

  read_q_size = 0;

  /* Acquire block buffers. */
  for (;;) {
  	block_t thisblock;
	read_q[read_q_size++] = bp;

	if (--blocks_ahead == 0) break;

	/* Don't trash the cache, leave 4 free. */
	if (lmfs_bufs_in_use() >= nr_bufs - 4) break;

	block++;
	position_running += block_size;

	if(!block_spec && 
	  (thisblock = read_map(rip, (off_t) ex64lo(position_running), 1)) != NO_BLOCK) {
	  	bp = lmfs_get_block_ino(dev, thisblock, PREFETCH, rip->i_num, position_running);
	} else {
		bp = get_block(dev, block, PREFETCH);
	}
	if (lmfs_dev(bp) != NO_DEV) {
		/* Oops, block already in the cache, get out. */
		put_block(bp, FULL_DATA_BLOCK);
		break;
	}
  }
  lmfs_rw_scattered(dev, read_q, read_q_size, READING);

  if(block_spec)
	  return get_block(dev, baseblock, NORMAL);
  return(lmfs_get_block_ino(dev, baseblock, NORMAL, rip->i_num, position));
}


/*===========================================================================*
 *				fs_getdents				     *
 *===========================================================================*/
int fs_getdents(void)
{
#define GETDENTS_BUFSIZE	(sizeof(struct dirent) + MFS_NAME_MAX + 1)
#define GETDENTS_ENTRIES	8
  static char getdents_buf[GETDENTS_BUFSIZE * GETDENTS_ENTRIES];
  register struct inode *rip;
  int o, r, done;
  unsigned int block_size, len, reclen;
  ino_t ino;
  cp_grant_id_t gid;
  size_t size, tmpbuf_off, userbuf_off;
  off_t pos, off, block_pos, new_pos, ent_pos;
  struct buf *bp;
  struct direct *dp;
  struct dirent *dep;
  char *cp;

  ino = (ino_t) fs_m_in.REQ_INODE_NR;
  gid = (gid_t) fs_m_in.REQ_GRANT;
  size = (size_t) fs_m_in.REQ_MEM_SIZE;
  pos = (off_t) fs_m_in.REQ_SEEK_POS_LO;

  /* Check whether the position is properly aligned */
  if( (unsigned int) pos % DIR_ENTRY_SIZE)
	  return(ENOENT);
  
  if( (rip = get_inode(fs_dev, ino)) == NULL) 
	  return(EINVAL);

  block_size = rip->i_sp->s_block_size;
  off = (pos % block_size);		/* Offset in block */
  block_pos = pos - off;
  done = FALSE;		/* Stop processing directory blocks when done is set */

  tmpbuf_off = 0;	/* Offset in getdents_buf */
  memset(getdents_buf, '\0', sizeof(getdents_buf));	/* Avoid leaking any data */
  userbuf_off = 0;	/* Offset in the user's buffer */

  /* The default position for the next request is EOF. If the user's buffer
   * fills up before EOF, new_pos will be modified. */
  new_pos = rip->i_size;

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
		
		  /* Compute record length */
		  reclen = offsetof(struct dirent, d_name) + len + 1;
		  o = (reclen % sizeof(long));
		  if (o != 0)
			  reclen += sizeof(long) - o;

		  /* Need the position of this entry in the directory */
		  ent_pos = block_pos + ((char *) dp - (char *) bp->data);

		if (userbuf_off + tmpbuf_off + reclen >= size) {
			  /* The user has no space for one more record */
			  done = TRUE;

			  /* Record the position of this entry, it is the
			   * starting point of the next request (unless the
			   * postion is modified with lseek).
			   */
			  new_pos = ent_pos;
			  break;
		}

		if (tmpbuf_off + reclen >= GETDENTS_BUFSIZE*GETDENTS_ENTRIES) {
			  r = sys_safecopyto(VFS_PROC_NR, gid,
			  		     (vir_bytes) userbuf_off, 
					     (vir_bytes) getdents_buf,
					     (size_t) tmpbuf_off);
			  if (r != OK) {
			  	put_inode(rip);
			  	return(r);
			  }

			  userbuf_off += tmpbuf_off;
			  tmpbuf_off = 0;
		}

		dep = (struct dirent *) &getdents_buf[tmpbuf_off];
		dep->d_ino = dp->mfs_d_ino;
		dep->d_off = ent_pos;
		dep->d_reclen = (unsigned short) reclen;
		memcpy(dep->d_name, dp->mfs_d_name, len);
		dep->d_name[len] = '\0';
		tmpbuf_off += reclen;
	}

	put_block(bp, DIRECTORY_BLOCK);
	if (done)
		break;
  }

  if (tmpbuf_off != 0) {
	r = sys_safecopyto(VFS_PROC_NR, gid, (vir_bytes) userbuf_off,
	  		     (vir_bytes) getdents_buf, (size_t) tmpbuf_off);
	if (r != OK) {
		put_inode(rip);
		return(r);
	}

	userbuf_off += tmpbuf_off;
  }

  if (done && userbuf_off == 0)
	  r = EINVAL;		/* The user's buffer is too small */
  else {
	  fs_m_out.RES_NBYTES = userbuf_off;
	  fs_m_out.RES_SEEK_POS_LO = new_pos;
	  if(!rip->i_sp->s_rd_only) {
		  rip->i_update |= ATIME;
		  IN_MARKDIRTY(rip);
	  }
	  r = OK;
  }

  put_inode(rip);		/* release the inode */
  return(r);
}

