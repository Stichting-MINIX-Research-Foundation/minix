/* Created (MFS based):
 *   February 2010 (Evgeniy Ivanov)
 */

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
#include <assert.h>
#include <sys/param.h>


static struct buf *rahead(struct inode *rip, block_t baseblock, u64_t
	position, unsigned bytes_ahead);
static int rw_chunk(struct inode *rip, u64_t position, unsigned off,
	size_t chunk, unsigned left, int rw_flag, cp_grant_id_t gid, unsigned
	buf_off, unsigned int block_size, int *completed);

static off_t rdahedpos;         /* position to read ahead */
static struct inode *rdahed_inode;      /* pointer to inode to read ahead */

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
	block_size = get_block_size( (dev_t) rip->i_block[0]);
	f_size = MAX_FILE_POS;
  } else {
	block_size = rip->i_sp->s_block_size;
	f_size = rip->i_size;
	if (f_size < 0) f_size = MAX_FILE_POS;
  }

  /* Get the values from the request message */
  rw_flag = (fs_m_in.m_type == REQ_READ ? READING : WRITING);
  gid = (cp_grant_id_t) fs_m_in.REQ_GRANT;
  position = (off_t) fs_m_in.REQ_SEEK_POS_LO;
  nrbytes = (size_t) fs_m_in.REQ_NBYTES;

  rdwt_err = OK;                /* set to EIO if disk error occurs */

  if (rw_flag == WRITING && !block_spec) {
	/* Check in advance to see if file will grow too big. */
	if (position > (off_t) (rip->i_sp->s_max_size - nrbytes))
		return(EFBIG);
  }

  cum_io = 0;
  /* Split the transfer into chunks that don't span two blocks. */
  while (nrbytes != 0) {
	off = (unsigned int) (position % block_size);/* offset in blk*/
	chunk = MIN(nrbytes, block_size - off);

	if (rw_flag == READING) {
		bytes_left = f_size - position;
		if (position >= f_size) break;        /* we are beyond EOF */
		if (chunk > bytes_left) chunk = (int) bytes_left;
	}

	/* Read or write 'chunk' bytes. */
	r = rw_chunk(rip, cvul64((unsigned long) position), off, chunk,
		     nrbytes, rw_flag, gid, cum_io, block_size, &completed);

	if (r != OK) break;   /* EOF reached */
	if (rdwt_err < 0) break;

	/* Update counters and pointers. */
	nrbytes -= chunk;     /* bytes yet to be read */
	cum_io += chunk;      /* bytes read so far */
	position += (off_t) chunk;    /* position within the file */
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
     (unsigned int) position % block_size == 0 &&
     (regular || mode_word == I_DIRECTORY)) {
	rdahed_inode = rip;
	rdahedpos = position;
  }

  rip->i_seek = NO_SEEK;

  if (rdwt_err != OK) r = rdwt_err;     /* check for disk error */
  if (rdwt_err == END_OF_FILE) r = OK;

  if (r == OK) {
	if (rw_flag == READING) rip->i_update |= ATIME;
	if (rw_flag == WRITING) rip->i_update |= CTIME | MTIME;
	rip->i_dirt = IN_DIRTY;          /* inode is thus now dirty */
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

  /* Pseudo inode for rw_chunk */
  struct inode rip;

  r = OK;

  /* Get the values from the request message */
  rw_flag = (fs_m_in.m_type == REQ_BREAD ? READING : WRITING);
  gid = (cp_grant_id_t) fs_m_in.REQ_GRANT;
  position = make64((unsigned long) fs_m_in.REQ_SEEK_POS_LO,
                    (unsigned long) fs_m_in.REQ_SEEK_POS_HI);
  nrbytes = (size_t) fs_m_in.REQ_NBYTES;

  block_size = get_block_size( (dev_t) fs_m_in.REQ_DEV2);

  rip.i_block[0] = (block_t) fs_m_in.REQ_DEV2;
  rip.i_mode = I_BLOCK_SPECIAL;
  rip.i_size = 0;

  rdwt_err = OK;                /* set to EIO if disk error occurs */

  cum_io = 0;
  /* Split the transfer into chunks that don't span two blocks. */
  while (nrbytes > 0) {
	  off = rem64u(position, block_size);	/* offset in blk*/
	  chunk = min(nrbytes, block_size - off);

	  /* Read or write 'chunk' bytes. */
	  r = rw_chunk(&rip, position, off, chunk, nrbytes, rw_flag, gid,
		       cum_io, block_size, &completed);

	  if (r != OK) break;	/* EOF reached */
	  if (rdwt_err < 0) break;

	  /* Update counters and pointers. */
	  nrbytes -= chunk;	        /* bytes yet to be read */
	  cum_io += chunk;	        /* bytes read so far */
	  position = add64ul(position, chunk);	/* position within the file */
  }

  fs_m_out.RES_SEEK_POS_LO = ex64lo(position);
  fs_m_out.RES_SEEK_POS_HI = ex64hi(position);

  if (rdwt_err != OK) r = rdwt_err;     /* check for disk error */
  if (rdwt_err == END_OF_FILE) r = OK;

  fs_m_out.RES_NBYTES = cum_io;

  return(r);
}


/*===========================================================================*
 *				rw_chunk				     *
 *===========================================================================*/
static int rw_chunk(rip, position, off, chunk, left, rw_flag, gid,
 buf_off, block_size, completed)
register struct inode *rip;     /* pointer to inode for file to be rd/wr */
u64_t position;                 /* position within file to read or write */
unsigned off;                   /* off within the current block */
unsigned int chunk;             /* number of bytes to read or write */
unsigned left;                  /* max number of bytes wanted after position */
int rw_flag;                    /* READING or WRITING */
cp_grant_id_t gid;              /* grant */
unsigned buf_off;               /* offset in grant */
unsigned int block_size;        /* block size of FS operating on */
int *completed;                 /* number of bytes copied */
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
	dev = (dev_t) rip->i_block[0];
  } else {
	if (ex64hi(position) != 0)
		panic("rw_chunk: position too high");
	b = read_map(rip, (off_t) ex64lo(position));
	dev = rip->i_dev;
  }

  if (!block_spec && b == NO_BLOCK) {
	if (rw_flag == READING) {
		/* Reading from a nonexistent block.  Must read as all zeros.*/
               r = sys_safememset(VFS_PROC_NR, gid, (vir_bytes) buf_off,
                          0, (size_t) chunk);
               if(r != OK) {
                       printf("ext2fs: sys_safememset failed\n");
               }
               return r;
	} else {
		/* Writing to a nonexistent block. Create and enter in inode.*/
		if ((bp = new_block(rip, (off_t) ex64lo(position))) == NULL)
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
	if (!block_spec && off == 0 && (off_t) ex64lo(position) >= rip->i_size)
		n = NO_READ;
	bp = get_block(dev, b, n);
  }

  /* In all cases, bp now points to a valid buffer. */
  if (bp == NULL)
	panic("bp not valid in rw_chunk, this can't happen");

  if (rw_flag == WRITING && chunk != block_size && !block_spec &&
      (off_t) ex64lo(position) >= rip->i_size && off == 0) {
	zero_block(bp);
  }

  if (rw_flag == READING) {
	/* Copy a chunk from the block buffer to user space. */
	r = sys_safecopyto(VFS_PROC_NR, gid, (vir_bytes) buf_off,
			   (vir_bytes) (b_data(bp)+off), (size_t) chunk);
  } else {
	/* Copy a chunk from user space to the block buffer. */
	r = sys_safecopyfrom(VFS_PROC_NR, gid, (vir_bytes) buf_off,
			     (vir_bytes) (b_data(bp)+off), (size_t) chunk);
	lmfs_markdirty(bp);
  }

  n = (off + chunk == block_size ? FULL_DATA_BLOCK : PARTIAL_DATA_BLOCK);
  put_block(bp, n);

  return(r);
}


/*===========================================================================*
 *				read_map				     *
 *===========================================================================*/
block_t read_map(rip, position)
register struct inode *rip;     /* ptr to inode to map from */
off_t position;                 /* position in file whose blk wanted */
{
/* Given an inode and a position within the corresponding file, locate the
 * block number in which that position is to be found and return it.
 */

  struct buf *bp;
  int index;
  block_t b;
  unsigned long excess, block_pos;
  static char first_time = TRUE;
  static long addr_in_block;
  static long addr_in_block2;
  static long doub_ind_s;
  static long triple_ind_s;
  static long out_range_s;

  if (first_time) {
	addr_in_block = rip->i_sp->s_block_size / BLOCK_ADDRESS_BYTES;
	addr_in_block2 = addr_in_block * addr_in_block;
	doub_ind_s = EXT2_NDIR_BLOCKS + addr_in_block;
	triple_ind_s = doub_ind_s + addr_in_block2;
	out_range_s = triple_ind_s + addr_in_block2 * addr_in_block;
	first_time = FALSE;
  }

  block_pos = position / rip->i_sp->s_block_size; /* relative blk # in file */

  /* Is 'position' to be found in the inode itself? */
  if (block_pos < EXT2_NDIR_BLOCKS)
	return(rip->i_block[block_pos]);

  /* It is not in the inode, so it must be single, double or triple indirect */
  if (block_pos < doub_ind_s) {
	b = rip->i_block[EXT2_NDIR_BLOCKS]; /* address of single indirect block */
	index = block_pos - EXT2_NDIR_BLOCKS;
  } else if (block_pos >= out_range_s) { /* TODO: do we need it? */
	return(NO_BLOCK);
  } else {
	/* double or triple indirect block. At first if it's triple,
	 * find double indirect block.
	 */
	excess = block_pos - doub_ind_s;
	b = rip->i_block[EXT2_DIND_BLOCK];
	if (block_pos >= triple_ind_s) {
		b = rip->i_block[EXT2_TIND_BLOCK];
		if (b == NO_BLOCK) return(NO_BLOCK);
		bp = get_block(rip->i_dev, b, NORMAL); /* get triple ind block */
		ASSERT(lmfs_dev(bp) != NO_DEV);
		ASSERT(lmfs_dev(bp) == rip->i_dev);
		excess = block_pos - triple_ind_s;
		index = excess / addr_in_block2;
		b = rd_indir(bp, index);	/* num of double ind block */
		put_block(bp, INDIRECT_BLOCK);	/* release triple ind block */
		excess = excess % addr_in_block2;
	}
	if (b == NO_BLOCK) return(NO_BLOCK);
	bp = get_block(rip->i_dev, b, NORMAL);	/* get double indirect block */
	ASSERT(lmfs_dev(bp) != NO_DEV);
	ASSERT(lmfs_dev(bp) == rip->i_dev);
	index = excess / addr_in_block;
	b = rd_indir(bp, index);	/* num of single ind block */
	put_block(bp, INDIRECT_BLOCK);	/* release double ind block */
	index = excess % addr_in_block;	/* index into single ind blk */
  }
  if (b == NO_BLOCK) return(NO_BLOCK);
  bp = get_block(rip->i_dev, b, NORMAL);
  ASSERT(lmfs_dev(bp) != NO_DEV);
  ASSERT(lmfs_dev(bp) == rip->i_dev);
  b = rd_indir(bp, index);
  put_block(bp, INDIRECT_BLOCK);	/* release single ind block */

  return(b);
}


/*===========================================================================*
 *				rd_indir				     *
 *===========================================================================*/
block_t rd_indir(bp, index)
struct buf *bp;                 /* pointer to indirect block */
int index;                      /* index into *bp */
{
  if (bp == NULL)
	panic("rd_indir() on NULL");
  /* TODO: use conv call */
  return conv4(le_CPU, b_ind(bp)[index]);
}


/*===========================================================================*
 *				read_ahead				     *
 *===========================================================================*/
void read_ahead()
{
/* Read a block into the cache before it is needed. */
  unsigned int block_size;
  register struct inode *rip;
  struct buf *bp;
  block_t b;

  if(!rdahed_inode)
	return;

  rip = rdahed_inode;           /* pointer to inode to read ahead from */
  block_size = get_block_size(rip->i_dev);
  rdahed_inode = NULL;     /* turn off read ahead */
  if ( (b = read_map(rip, rdahedpos)) == NO_BLOCK) return;      /* at EOF */

  assert(rdahedpos >= 0); /* So we can safely cast it to unsigned below */

  bp = rahead(rip, b, cvul64((unsigned long) rdahedpos), block_size);
  put_block(bp, PARTIAL_DATA_BLOCK);
}


/*===========================================================================*
 *				rahead					     *
 *===========================================================================*/
static struct buf *rahead(rip, baseblock, position, bytes_ahead)
register struct inode *rip;     /* pointer to inode for file to be read */
block_t baseblock;              /* block at current position */
u64_t position;                 /* position within file */
unsigned bytes_ahead;           /* bytes beyond position for immediate use */
{
/* Fetch a block from the cache or the device.  If a physical read is
 * required, prefetch as many more blocks as convenient into the cache.
 * This usually covers bytes_ahead and is at least BLOCKS_MINIMUM.
 * The device driver may decide it knows better and stop reading at a
 * cylinder boundary (or after an error).  Rw_scattered() puts an optional
 * flag on all reads to allow this.
 */
/* Minimum number of blocks to prefetch. */
# define BLOCKS_MINIMUM		(nr_bufs < 50 ? 18 : 32)
  int nr_bufs = lmfs_nr_bufs();
  int block_spec, read_q_size;
  unsigned int blocks_ahead, fragment, block_size;
  block_t block, blocks_left;
  off_t ind1_pos;
  dev_t dev;
  struct buf *bp = NULL;
  static unsigned int readqsize = 0;
  static struct buf **read_q = NULL;

  if(readqsize != nr_bufs) {
	if(readqsize > 0) {
		assert(read_q != NULL);
		free(read_q);
		read_q = NULL;
		readqsize = 0;
	} 

	assert(readqsize == 0);
	assert(read_q == NULL);

	if(!(read_q = malloc(sizeof(read_q[0])*nr_bufs)))
		panic("couldn't allocate read_q");
	readqsize = nr_bufs;
  }

  block_spec = (rip->i_mode & I_TYPE) == I_BLOCK_SPECIAL;
  if (block_spec)
	dev = (dev_t) rip->i_block[0];
  else
	dev = rip->i_dev;

  block_size = get_block_size(dev);

  block = baseblock;
  bp = get_block(dev, block, PREFETCH);
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
   * better solution must look at the already available
   * indirect blocks (but don't call read_map!).
   */

  fragment = rem64u(position, block_size);
  position = sub64u(position, fragment);
  bytes_ahead += fragment;

  blocks_ahead = (bytes_ahead + block_size - 1) / block_size;

  if (block_spec && rip->i_size == 0) {
	blocks_left = (block_t) NR_IOREQS;
  } else {
	blocks_left = (block_t) (rip->i_size-ex64lo(position)+(block_size-1)) /
                                                                block_size;

	/* Go for the first indirect block if we are in its neighborhood. */
	if (!block_spec) {
		ind1_pos = (EXT2_NDIR_BLOCKS) * block_size;
		if ((off_t) ex64lo(position) <= ind1_pos && rip->i_size > ind1_pos) {
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
	if (lmfs_bufs_in_use() >= nr_bufs - 4) break;

	block++;

	bp = get_block(dev, block, PREFETCH);
	if (lmfs_dev(bp) != NO_DEV) {
		/* Oops, block already in the cache, get out. */
		put_block(bp, FULL_DATA_BLOCK);
		break;
	}
  }
  lmfs_rw_scattered(dev, read_q, read_q_size, READING);
  return(get_block(dev, baseblock, NORMAL));
}


/*===========================================================================*
 *				fs_getdents				     *
 *===========================================================================*/
int fs_getdents(void)
{
#define GETDENTS_BUFSIZE (sizeof(struct dirent) + EXT2_NAME_MAX + 1)
#define GETDENTS_ENTRIES	8
  static char getdents_buf[GETDENTS_BUFSIZE * GETDENTS_ENTRIES];
  struct inode *rip;
  int o, r, done;
  unsigned int block_size, len, reclen;
  ino_t ino;
  block_t b;
  cp_grant_id_t gid;
  size_t size, tmpbuf_off, userbuf_off;
  off_t pos, off, block_pos, new_pos, ent_pos;
  struct buf *bp;
  struct ext2_disk_dir_desc *d_desc;
  struct dirent *dep;

  ino = (ino_t) fs_m_in.REQ_INODE_NR;
  gid = (gid_t) fs_m_in.REQ_GRANT;
  size = (size_t) fs_m_in.REQ_MEM_SIZE;
  pos = (off_t) fs_m_in.REQ_SEEK_POS_LO;

  /* Check whether the position is properly aligned */
  if ((unsigned int) pos % DIR_ENTRY_ALIGN)
	return(ENOENT);

  if ((rip = get_inode(fs_dev, ino)) == NULL)
	return(EINVAL);

  block_size = rip->i_sp->s_block_size;
  off = (pos % block_size);             /* Offset in block */
  block_pos = pos - off;
  done = FALSE;       /* Stop processing directory blocks when done is set */

  memset(getdents_buf, '\0', sizeof(getdents_buf));  /* Avoid leaking any data */
  tmpbuf_off = 0;       /* Offset in getdents_buf */
  userbuf_off = 0;      /* Offset in the user's buffer */

  /* The default position for the next request is EOF. If the user's buffer
   * fills up before EOF, new_pos will be modified. */
  new_pos = rip->i_size;

  for (; block_pos < rip->i_size; block_pos += block_size) {
	off_t temp_pos = block_pos;
	b = read_map(rip, block_pos); /* get block number */
	/* Since directories don't have holes, 'b' cannot be NO_BLOCK. */
	bp = get_block(rip->i_dev, b, NORMAL);  /* get a dir block */
	assert(bp != NULL);

	/* Search a directory block. */
	d_desc = (struct ext2_disk_dir_desc*) &b_data(bp);

	/* we need to seek to entry at off bytes.
	* when NEXT_DISC_DIR_POS == block_size it's last dentry.
	*/
	for (; temp_pos + conv2(le_CPU, d_desc->d_rec_len) <= pos
	       && NEXT_DISC_DIR_POS(d_desc, &b_data(bp)) < block_size;
	       d_desc = NEXT_DISC_DIR_DESC(d_desc)) {
		temp_pos += conv2(le_CPU, d_desc->d_rec_len);
	}

	for (; CUR_DISC_DIR_POS(d_desc, &b_data(bp)) < block_size;
	     d_desc = NEXT_DISC_DIR_DESC(d_desc)) {
		if (d_desc->d_ino == 0)
			continue; /* Entry is not in use */

		if (d_desc->d_name_len > NAME_MAX ||
		    d_desc->d_name_len > EXT2_NAME_MAX) {
			len = min(NAME_MAX, EXT2_NAME_MAX);
		} else {
			len = d_desc->d_name_len;
		}

		/* Compute record length */
		reclen = offsetof(struct dirent, d_name) + len + 1;
		o = (reclen % sizeof(long));
		if (o != 0)
			reclen += sizeof(long) - o;

		/* Need the position of this entry in the directory */
		ent_pos = block_pos + ((char *)d_desc - b_data(bp));

		if (userbuf_off + tmpbuf_off + reclen >= size) {
			/* The user has no space for one more record */
			done = TRUE;

			/* Record the position of this entry, it is the
			 * starting point of the next request (unless the
			 * position is modified with lseek).
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
		dep->d_ino = conv4(le_CPU, d_desc->d_ino);
		dep->d_off = ent_pos;
		dep->d_reclen = (unsigned short) reclen;
		memcpy(dep->d_name, d_desc->d_name, len);
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
	r = EINVAL;           /* The user's buffer is too small */
  else {
	fs_m_out.RES_NBYTES = userbuf_off;
	fs_m_out.RES_SEEK_POS_LO = new_pos;
	rip->i_update |= ATIME;
	rip->i_dirt = IN_DIRTY;
	r = OK;
  }

  put_inode(rip);               /* release the inode */
  return(r);
}
