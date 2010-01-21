#include "fs.h"
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <minix/com.h>
#include <minix/u64.h>
#include "buf.h"
#include "inode.h"
#include "super.h"
#include <minix/vfsif.h>

FORWARD _PROTOTYPE( int rw_chunk, (struct inode *rip, u64_t position,
	unsigned off, int chunk, unsigned left, int rw_flag,
	cp_grant_id_t gid, unsigned buf_off, int block_size, int *completed));

PRIVATE char getdents_buf[GETDENTS_BUFSIZ];

/*===========================================================================*
 *				fs_readwrite				     *
 *===========================================================================*/
PUBLIC int fs_readwrite(void)
{
  int r, rw_flag, chunk, block_size, block_spec;
  int partial_cnt, regular, nrbytes;
  cp_grant_id_t gid;
  off_t position, f_size, bytes_left;
  unsigned int off, cum_io;
  mode_t mode_word;
  int completed, r2 = OK;
  struct inode *rip;
  
  r = OK;
  
  /* Find the inode referred */
  if ((rip = find_inode(fs_dev, fs_m_in.REQ_INODE_NR)) == NIL_INODE)
	return(EINVAL);

  mode_word = rip->i_mode & I_TYPE;
  regular = (mode_word == I_REGULAR || mode_word == I_NAMED_PIPE);
  block_spec = (mode_word == I_BLOCK_SPECIAL ? 1 : 0);
  
  /* Determine blocksize */
  block_size = (block_spec ?
		get_block_size(rip->i_zone[0]) : rip->i_sp->s_block_size);

  f_size = (block_spec ? ULONG_MAX : rip->i_size);
  
  /* Get the values from the request message */ 
  rw_flag = (fs_m_in.m_type == REQ_READ ? READING : WRITING);
  gid = fs_m_in.REQ_GRANT;
  position = fs_m_in.REQ_SEEK_POS_LO;
  nrbytes = (unsigned) fs_m_in.REQ_NBYTES;
  
  rdwt_err = OK;		/* set to EIO if disk error occurs */
  
  if (rw_flag == WRITING && block_spec == 0) {
	  /* Check in advance to see if file will grow too big. */
	  if (position > rip->i_sp->s_max_size - nrbytes)
		  return(EFBIG);

	  /* Clear the zone containing present EOF if hole about
	   * to be created.  This is necessary because all unwritten
	   * blocks prior to the EOF must read as zeros. */
	  if(position > f_size) clear_zone(rip, f_size, 0);
  }
	      
  cum_io = 0;
  /* Split the transfer into chunks that don't span two blocks. */
  while (nrbytes != 0) {
	  off = (unsigned int) (position % block_size);/* offset in blk*/
	  chunk = MIN(nrbytes, block_size - off);

	  if (chunk < 0) chunk = block_size - off;

	  if (rw_flag == READING) {
		  bytes_left = f_size - position;
		  if (position >= f_size) break;	/* we are beyond EOF */
		  if (chunk > bytes_left) chunk = (int) bytes_left;
	  }
	  
	  /* Read or write 'chunk' bytes. */
	  r = rw_chunk(rip, cvul64(position), off, chunk, (unsigned) nrbytes,
			 rw_flag, gid, cum_io, block_size, &completed);

	  if (r != OK) break;	/* EOF reached */
	  if (rdwt_err < 0) break;

	  /* Update counters and pointers. */
	  nrbytes -= chunk;	/* bytes yet to be read */
	  cum_io += chunk;	/* bytes read so far */
	  position += chunk;	/* position within the file */
  }

  fs_m_out.RES_SEEK_POS_LO = position; /* It might change later and the VFS
					   has to know this value */
  
  /* On write, update file size and access time. */
  if (rw_flag == WRITING) {
	  if (regular || mode_word == I_DIRECTORY) {
		  if (position > f_size) rip->i_size = position;
	  }
  } 

  /* Check to see if read-ahead is called for, and if so, set it up. */
  if(rw_flag == READING && rip->i_seek == NO_SEEK &&
     position % block_size == 0 && (regular || mode_word == I_DIRECTORY)) {
	  rdahed_inode = rip;
	  rdahedpos = position;
  }

  rip->i_seek = NO_SEEK;

  if (rdwt_err != OK) r = rdwt_err;	/* check for disk error */
  if (rdwt_err == END_OF_FILE) r = OK;

  /* if user-space copying failed, read/write failed. */
  if (r == OK && r2 != OK)
	r = r2;
  
  if (r == OK) {
	  if (rw_flag == READING) rip->i_update |= ATIME;
	  if (rw_flag == WRITING) rip->i_update |= CTIME | MTIME;
	  rip->i_dirt = DIRTY;		/* inode is thus now dirty */
  }
  
  fs_m_out.RES_NBYTES = cum_io;
  
  return(r);
}


/*===========================================================================*
 *				fs_breadwrite				     *
 *===========================================================================*/
PUBLIC int fs_breadwrite(void)
{
  int r, rw_flag, chunk, block_size;
  cp_grant_id_t gid;
  int nrbytes;
  u64_t position;
  unsigned int off, cum_io;
  mode_t mode_word;
  int completed, r2 = OK;

  /* Pseudo inode for rw_chunk */
  struct inode rip;
  
  r = OK;
  
  /* Get the values from the request message */ 
  rw_flag = (fs_m_in.m_type == REQ_BREAD ? READING : WRITING);
  gid = fs_m_in.REQ_GRANT;
  position = make64(fs_m_in.REQ_SEEK_POS_LO, fs_m_in.REQ_SEEK_POS_HI);
  nrbytes = (unsigned) fs_m_in.REQ_NBYTES;
  
  block_size = get_block_size(fs_m_in.REQ_DEV2);

  rip.i_zone[0] = fs_m_in.REQ_DEV2;
  rip.i_mode = I_BLOCK_SPECIAL;
  rip.i_size = 0;

  rdwt_err = OK;		/* set to EIO if disk error occurs */
  
  cum_io = 0;
  /* Split the transfer into chunks that don't span two blocks. */
  while (nrbytes != 0) {
	  off = rem64u(position, block_size);	/* offset in blk*/
        
	  chunk = MIN(nrbytes, block_size - off);
	  if (chunk < 0) chunk = block_size - off;

	  /* Read or write 'chunk' bytes. */
	  r = rw_chunk(&rip, position, off, chunk, (unsigned) nrbytes,
			 rw_flag, gid, cum_io, block_size, &completed);

	  if (r != OK) break;	/* EOF reached */
	  if (rdwt_err < 0) break;

	  /* Update counters and pointers. */
	  nrbytes -= chunk;	        /* bytes yet to be read */
	  cum_io += chunk;	        /* bytes read so far */
	  position= add64ul(position, chunk);	/* position within the file */
  }
  
  fs_m_out.RES_SEEK_POS_LO = ex64lo(position); 
  fs_m_out.RES_SEEK_POS_HI = ex64hi(position); 
  
  if (rdwt_err != OK) r = rdwt_err;	/* check for disk error */
  if (rdwt_err == END_OF_FILE) r = OK;

  fs_m_out.RES_NBYTES = cum_io;
  
  return(r);
}


/*===========================================================================*
 *				rw_chunk				     *
 *===========================================================================*/
PRIVATE int rw_chunk(rip, position, off, chunk, left, rw_flag, gid,
 buf_off, block_size, completed)
register struct inode *rip;	/* pointer to inode for file to be rd/wr */
u64_t position;			/* position within file to read or write */
unsigned off;			/* off within the current block */
int chunk;			/* number of bytes to read or write */
unsigned left;			/* max number of bytes wanted after position */
int rw_flag;			/* READING or WRITING */
cp_grant_id_t gid;		/* grant */
unsigned buf_off;		/* offset in grant */
int block_size;			/* block size of FS operating on */
int *completed;			/* number of bytes copied */
{
/* Read or write (part of) a block. */

  register struct buf *bp;
  register int r = OK;
  int n, block_spec;
  block_t b;
  dev_t dev;

  *completed = 0;

  block_spec = (rip->i_mode & I_TYPE) == I_BLOCK_SPECIAL;

  if (block_spec) {
	b = div64u(position, block_size);
	dev = (dev_t) rip->i_zone[0];
  } else {
	if (ex64hi(position) != 0)
		panic(__FILE__, "rw_chunk: position too high", NO_NUM);
	b = read_map(rip, ex64lo(position));
	dev = rip->i_dev;
  }

  if (!block_spec && b == NO_BLOCK) {
	if (rw_flag == READING) {
		/* Reading from a nonexistent block.  Must read as all zeros.*/
		bp = get_block(NO_DEV, NO_BLOCK, NORMAL);    /* get a buffer */
		zero_block(bp);
	} else {
		/* Writing to a nonexistent block. Create and enter in inode.*/
		if ((bp= new_block(rip, ex64lo(position))) == NIL_BUF)
			return(err_code);
	}
  } else if (rw_flag == READING) {
	/* Read and read ahead if convenient. */
	bp = rahead(rip, b, position, left);
  } else {
	/* Normally an existing block to be partially overwritten is first read
	 * in.  However, a full block need not be read in.  If it is already in
	 * the cache, acquire it, otherwise just acquire a free buffer.
	 */
	n = (chunk == block_size ? NO_READ : NORMAL);
	if (!block_spec && off == 0 && ex64lo(position) >= rip->i_size) 
		n = NO_READ;
	bp = get_block(dev, b, n);
  }

  /* In all cases, bp now points to a valid buffer. */
  if (bp == NIL_BUF) 
  	panic(__FILE__,"bp not valid in rw_chunk, this can't happen", NO_NUM);
  
  if (rw_flag == WRITING && chunk != block_size && !block_spec &&
				ex64lo(position) >= rip->i_size && off == 0) {
	zero_block(bp);
  }

  if (rw_flag == READING) {
	/* Copy a chunk from the block buffer to user space. */
	r = sys_safecopyto(FS_PROC_NR, gid, buf_off,
		(vir_bytes) (bp->b_data+off), (phys_bytes) chunk, D);
  } else {
	/* Copy a chunk from user space to the block buffer. */
	r = sys_safecopyfrom(FS_PROC_NR, gid, buf_off,
		(vir_bytes) (bp->b_data+off), (phys_bytes) chunk, D);
	bp->b_dirt = DIRTY;
  }
  
  n = (off + chunk == block_size ? FULL_DATA_BLOCK : PARTIAL_DATA_BLOCK);
  put_block(bp, n);

  return(r);
}


/*===========================================================================*
 *				read_map				     *
 *===========================================================================*/
PUBLIC block_t read_map(rip, position)
register struct inode *rip;	/* ptr to inode to map from */
off_t position;			/* position in file whose blk wanted */
{
/* Given an inode and a position within the corresponding file, locate the
 * block (not zone) number in which that position is to be found and return it.
 */

  register struct buf *bp;
  register zone_t z;
  int scale, boff, dzones, nr_indirects, index, zind, ex;
  block_t b;
  long excess, zone, block_pos;
  
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
	b = ((block_t) z << scale) + boff;
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
	bp = get_block(rip->i_dev, b, NORMAL);	/* get double indirect block */
	index = (int) (excess/nr_indirects);
	ASSERT(bp->b_dev != NO_DEV);
	ASSERT(bp->b_dev == rip->i_dev);
	z = rd_indir(bp, index);		/* z= zone for single*/
	put_block(bp, INDIRECT_BLOCK);		/* release double ind block */
	excess = excess % nr_indirects;		/* index into single ind blk */
  }

  /* 'z' is zone num for single indirect block; 'excess' is index into it. */
  if (z == NO_ZONE) return(NO_BLOCK);
  b = (block_t) z << scale;			/* b is blk # for single ind */
  bp = get_block(rip->i_dev, b, NORMAL);	/* get single indirect block */
  ex = (int) excess;				/* need an integer */
  z = rd_indir(bp, ex);				/* get block pointed to */
  put_block(bp, INDIRECT_BLOCK);		/* release single indir blk */
  if (z == NO_ZONE) return(NO_BLOCK);
  b = ((block_t) z << scale) + boff;
  return(b);
}


/*===========================================================================*
 *				rd_indir				     *
 *===========================================================================*/
PUBLIC zone_t rd_indir(bp, index)
struct buf *bp;			/* pointer to indirect block */
int index;			/* index into *bp */
{
/* Given a pointer to an indirect block, read one entry.  The reason for
 * making a separate routine out of this is that there are four cases:
 * V1 (IBM and 68000), and V2 (IBM and 68000).
 */

  struct super_block *sp;
  zone_t zone;			/* V2 zones are longs (shorts in V1) */

  if(bp == NIL_BUF)
	panic(__FILE__, "rd_indir() on NIL_BUF", NO_NUM);

  sp = get_super(bp->b_dev);	/* need super block to find file sys type */

  /* read a zone from an indirect block */
  if (sp->s_version == V1)
	zone = (zone_t) conv2(sp->s_native, (int)  bp->b_v1_ind[index]);
  else
	zone = (zone_t) conv4(sp->s_native, (long) bp->b_v2_ind[index]);

  if (zone != NO_ZONE &&
		(zone < (zone_t) sp->s_firstdatazone || zone >= sp->s_zones)) {
	printf("Illegal zone number %ld in indirect block, index %d\n",
	       (long) zone, index);
	panic(__FILE__,"check file system", NO_NUM);
  }
  
  return(zone);
}


/*===========================================================================*
 *				read_ahead				     *
 *===========================================================================*/
PUBLIC void read_ahead()
{
/* Read a block into the cache before it is needed. */
  int block_size;
  register struct inode *rip;
  struct buf *bp;
  block_t b;

  rip = rdahed_inode;		/* pointer to inode to read ahead from */
  block_size = get_block_size(rip->i_dev);
  rdahed_inode = NIL_INODE;	/* turn off read ahead */
  if ( (b = read_map(rip, rdahedpos)) == NO_BLOCK) return;	/* at EOF */
  bp = rahead(rip, b, cvul64(rdahedpos), block_size);
  put_block(bp, PARTIAL_DATA_BLOCK);
}


/*===========================================================================*
 *				rahead					     *
 *===========================================================================*/
PUBLIC struct buf *rahead(rip, baseblock, position, bytes_ahead)
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
  int block_size;
/* Minimum number of blocks to prefetch. */
# define BLOCKS_MINIMUM		(NR_BUFS < 50 ? 18 : 32)
  int block_spec, scale, read_q_size;
  unsigned int blocks_ahead, fragment;
  block_t block, blocks_left;
  off_t ind1_pos;
  dev_t dev;
  struct buf *bp;
  static struct buf **read_q;

  STATICINIT(read_q, NR_BUFS);

  block_spec = (rip->i_mode & I_TYPE) == I_BLOCK_SPECIAL;
  if (block_spec) 
	dev = (dev_t) rip->i_zone[0];
  else 
	dev = rip->i_dev;
  
  block_size = get_block_size(dev);

  block = baseblock;
  bp = get_block(dev, block, PREFETCH);
  if (bp->b_dev != NO_DEV) return(bp);

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

  fragment = rem64u(position, block_size);
  position= sub64u(position, fragment);
  bytes_ahead += fragment;

  blocks_ahead = (bytes_ahead + block_size - 1) / block_size;

  if (block_spec && rip->i_size == 0) {
	blocks_left = NR_IOREQS;
  } else {
	blocks_left = (rip->i_size - ex64lo(position) + block_size - 1) /
		block_size;

	/* Go for the first indirect block if we are in its neighborhood. */
	if (!block_spec) {
		scale = rip->i_sp->s_log_zone_size;
		ind1_pos = (off_t) rip->i_ndzones * (block_size << scale);
		if (ex64lo(position) <= ind1_pos && rip->i_size > ind1_pos) {
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
	read_q[read_q_size++] = bp;

	if (--blocks_ahead == 0) break;

	/* Don't trash the cache, leave 4 free. */
	if (bufs_in_use >= NR_BUFS - 4) break;

	block++;

	bp = get_block(dev, block, PREFETCH);
	if (bp->b_dev != NO_DEV) {
		/* Oops, block already in the cache, get out. */
		put_block(bp, FULL_DATA_BLOCK);
		break;
	}
  }
  rw_scattered(dev, read_q, read_q_size, READING);
  return(get_block(dev, baseblock, NORMAL));
}


/*===========================================================================*
 *				fs_getdents				     *
 *===========================================================================*/
PUBLIC int fs_getdents(void)
{
  register struct inode *rip;
  int o, r, block_size, len, reclen, done;
  ino_t ino;
  block_t b;
  cp_grant_id_t gid;
  size_t size, tmpbuf_off, userbuf_off;
  off_t pos, off, block_pos, new_pos, ent_pos;
  struct buf *bp;
  struct direct *dp;
  struct dirent *dep;
  char *cp;

  ino = fs_m_in.REQ_INODE_NR;
  gid = fs_m_in.REQ_GRANT;
  size = fs_m_in.REQ_MEM_SIZE;
  pos = fs_m_in.REQ_SEEK_POS_LO;

  /* Check whether the position is properly aligned */
  if(pos % DIR_ENTRY_SIZE)
	  return(ENOENT);
  
  if( (rip = get_inode(fs_dev, ino)) == NIL_INODE) 
	  return(EINVAL);

  block_size = rip->i_sp->s_block_size;
  off = (pos % block_size);		/* Offset in block */
  block_pos = pos - off;
  done = FALSE;		/* Stop processing directory blocks when done is set */

  tmpbuf_off = 0;	/* Offset in getdents_buf */
  memset(getdents_buf, '\0', GETDENTS_BUFSIZ);	/* Avoid leaking any data */
  userbuf_off = 0;	/* Offset in the user's buffer */

  /* The default position for the next request is EOF. If the user's buffer
   * fills up before EOF, new_pos will be modified. */
  new_pos = rip->i_size;

  for(; block_pos < rip->i_size; block_pos += block_size) {
	  b = read_map(rip, block_pos);	/* get block number */
	  
	  /* Since directories don't have holes, 'b' cannot be NO_BLOCK. */
	  bp = get_block(rip->i_dev, b, NORMAL);	/* get a dir block */

	  if(bp == NO_BLOCK)
		  panic(__FILE__,"get_block returned NO_BLOCK", NO_NUM);

	  /* Search a directory block. */
	  if (block_pos < pos)
		  dp = &bp->b_dir[off / DIR_ENTRY_SIZE];
	  else
		  dp = &bp->b_dir[0];
	  for (; dp < &bp->b_dir[NR_DIR_ENTRIES(block_size)]; dp++) {
		  if (dp->d_ino == 0) 
			  continue;	/* Entry is not in use */

		  /* Compute the length of the name */
		  cp = memchr(dp->d_name, '\0', NAME_MAX);
		  if (cp == NULL)
			  len = NAME_MAX;
		  else
			  len = cp-dp->d_name;
		
		  /* Compute record length */
		  reclen = offsetof(struct dirent, d_name) + len + 1;
		  o = (reclen % sizeof(long));
		  if (o != 0)
			  reclen += sizeof(long) - o;

		  /* Need the position of this entry in the directory */
		  ent_pos = block_pos + ((char *)dp - bp->b_data);

		  if(tmpbuf_off + reclen > GETDENTS_BUFSIZ) {
			  r = sys_safecopyto(FS_PROC_NR, gid, userbuf_off, 
					     (vir_bytes)getdents_buf,
					     tmpbuf_off, D);
			  if (r != OK)
				  panic(__FILE__,
					"fs_getdents: sys_safecopyto failed\n",
					r);

			  userbuf_off += tmpbuf_off;
			  tmpbuf_off = 0;
		  }
		  
		  if(userbuf_off + tmpbuf_off + reclen > size) {
			  /* The user has no space for one more record */
			  done = TRUE;
			  
			  /* Record the position of this entry, it is the
			   * starting point of the next request (unless the
			   * postion is modified with lseek).
			   */
			  new_pos = ent_pos;
			  break;
		  }

		  dep = (struct dirent *)&getdents_buf[tmpbuf_off];
		  dep->d_ino = dp->d_ino;
		  dep->d_off = ent_pos;
		  dep->d_reclen = reclen;
		  memcpy(dep->d_name, dp->d_name, len);
		  dep->d_name[len] = '\0';
		  tmpbuf_off += reclen;
	  }

	  put_block(bp, DIRECTORY_BLOCK);
	  if(done)
		  break;
  }

  if(tmpbuf_off != 0) {
	  r = sys_safecopyto(FS_PROC_NR, gid, userbuf_off, 
		(vir_bytes) getdents_buf, tmpbuf_off, D);
	  if (r != OK)
		  panic(__FILE__, "fs_getdents: sys_safecopyto failed\n", r);

	  userbuf_off += tmpbuf_off;
  }

  if(done && userbuf_off == 0)
	  r = EINVAL;		/* The user's buffer is too small */
  else {
	  fs_m_out.RES_NBYTES = userbuf_off;
	  fs_m_out.RES_SEEK_POS_LO = new_pos;
	  rip->i_update |= ATIME;
	  rip->i_dirt = DIRTY;
	  r = OK;
  }

  put_inode(rip);		/* release the inode */
  return(r);
}

