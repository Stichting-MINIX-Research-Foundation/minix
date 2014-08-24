/* Created (MFS based):
 *   February 2010 (Evgeniy Ivanov)
 */

#include "fs.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "buf.h"
#include "inode.h"
#include "super.h"
#include <sys/param.h>
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
  if (f_size < 0) f_size = MAX_FILE_POS;

  lmfs_reset_rdwt_err();

  if (call == FSC_WRITE) {
	/* Check in advance to see if file will grow too big. */
	if (position > (off_t) (rip->i_sp->s_max_size - nrbytes))
		return(EFBIG);
  }

  cum_io = 0;
  /* Split the transfer into chunks that don't span two blocks. */
  while (nrbytes != 0) {
	off = (unsigned int) (position % block_size);/* offset in blk*/
	chunk = block_size - off;
	if (chunk > nrbytes)
		chunk = nrbytes;

	if (call == FSC_READ) {
		bytes_left = f_size - position;
		if (position >= f_size) break;        /* we are beyond EOF */
		if (chunk > bytes_left) chunk = (int) bytes_left;
	}

	/* Read or write 'chunk' bytes. */
	r = rw_chunk(rip, ((u64_t)((unsigned long)position)), off, chunk,
		nrbytes, call, data, cum_io, block_size, &completed);

	if (r != OK) break;   /* EOF reached */
	if (lmfs_rdwt_err() < 0) break;

	/* Update counters and pointers. */
	nrbytes -= chunk;     /* bytes yet to be read */
	cum_io += chunk;      /* bytes read so far */
	position += (off_t) chunk;    /* position within the file */
  }

  /* On write, update file size and access time. */
  if (call == FSC_WRITE) {
	if (regular || mode_word == I_DIRECTORY) {
		if (position > f_size) rip->i_size = position;
        }
  }

  rip->i_seek = NO_SEEK;

  if (lmfs_rdwt_err() != OK) r = lmfs_rdwt_err(); /* check for disk error */
  if (lmfs_rdwt_err() == END_OF_FILE) r = OK;

  if (r != OK)
	return r;

  if (call == FSC_READ) rip->i_update |= ATIME;
  if (call == FSC_WRITE) rip->i_update |= CTIME | MTIME;
  rip->i_dirt = IN_DIRTY;          /* inode is thus now dirty */

  return(cum_io);
}


/*===========================================================================*
 *				rw_chunk				     *
 *===========================================================================*/
static int rw_chunk(rip, position, off, chunk, left, call, data, buf_off,
	block_size, completed)
register struct inode *rip;     /* pointer to inode for file to be rd/wr */
u64_t position;                 /* position within file to read or write */
unsigned off;                   /* off within the current block */
size_t chunk;                   /* number of bytes to read or write */
unsigned left;                  /* max number of bytes wanted after position */
int call;                       /* FSC_READ, FSC_WRITE, or FSC_PEEK */
struct fsdriver_data *data;     /* structure for (remote) user buffer */
unsigned buf_off;               /* offset in user buffer */
unsigned int block_size;        /* block size of FS operating on */
int *completed;                 /* number of bytes copied */
{
/* Read or write (part of) a block. */

  register struct buf *bp = NULL;
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
			printf("ext2fs: fsdriver_zero failed\n");
		}
		return r;
	} else {
               /* Writing to or peeking a nonexistent block.
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
	bp = lmfs_get_block_ino(dev, b, n, ino, ino_off);
  }

  /* In all cases, bp now points to a valid buffer. */
  if (bp == NULL)
	panic("bp not valid in rw_chunk, this can't happen");

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
	lmfs_markdirty(bp);
  }

  n = (off + chunk == block_size ? FULL_DATA_BLOCK : PARTIAL_DATA_BLOCK);
  put_block(bp, n);

  return(r);
}


/*===========================================================================*
 *				read_map				     *
 *===========================================================================*/
block_t read_map(rip, position, opportunistic)
register struct inode *rip;     /* ptr to inode to map from */
off_t position;                 /* position in file whose blk wanted */
int opportunistic;
{
/* Given an inode and a position within the corresponding file, locate the
 * block number in which that position is to be found and return it.
 */

  struct buf *bp;
  int mindex;
  block_t b;
  unsigned long excess, block_pos;
  static char first_time = TRUE;
  static long addr_in_block;
  static long addr_in_block2;
  static long doub_ind_s;
  static long triple_ind_s;
  static long out_range_s;
  int iomode = NORMAL;
 
  if(opportunistic) iomode = PREFETCH;

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
	mindex = block_pos - EXT2_NDIR_BLOCKS;
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
		mindex = excess / addr_in_block2;
		b = rd_indir(bp, mindex);	/* num of double ind block */
		put_block(bp, INDIRECT_BLOCK);	/* release triple ind block */
		excess = excess % addr_in_block2;
	}
	if (b == NO_BLOCK) return(NO_BLOCK);
	bp = get_block(rip->i_dev, b, iomode); /* get double indirect block */
	if(opportunistic && lmfs_dev(bp) == NO_DEV) {
		put_block(bp, INDIRECT_BLOCK);
		return NO_BLOCK;
	}
	ASSERT(lmfs_dev(bp) != NO_DEV);
	ASSERT(lmfs_dev(bp) == rip->i_dev);
	mindex = excess / addr_in_block;
	b = rd_indir(bp, mindex);	/* num of single ind block */
	put_block(bp, INDIRECT_BLOCK);	/* release double ind block */
	mindex = excess % addr_in_block;	/* index into single ind blk */
  }
  if (b == NO_BLOCK) return(NO_BLOCK);
  bp = get_block(rip->i_dev, b, iomode);       /* get single indirect block */
  if(opportunistic && lmfs_dev(bp) == NO_DEV) {
       put_block(bp, INDIRECT_BLOCK);
       return NO_BLOCK;
  }

  ASSERT(lmfs_dev(bp) != NO_DEV);
  ASSERT(lmfs_dev(bp) == rip->i_dev);
  b = rd_indir(bp, mindex);
  put_block(bp, INDIRECT_BLOCK);	/* release single ind block */

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
block_t rd_indir(bp, mindex)
struct buf *bp;                 /* pointer to indirect block */
int mindex;                      /* index into *bp */
{
  if (bp == NULL)
	panic("rd_indir() on NULL");
  /* TODO: use conv call */
  return conv4(le_CPU, b_ind(bp)[mindex]);
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
  int read_q_size;
  unsigned int blocks_ahead, fragment, block_size;
  block_t block, blocks_left;
  off_t ind1_pos;
  dev_t dev;
  struct buf *bp = NULL;
  static unsigned int readqsize = 0;
  static struct buf **read_q = NULL;
  u64_t position_running;

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

  dev = rip->i_dev;
  assert(dev != NO_DEV);
  block_size = get_block_size(dev);

  block = baseblock;

  fragment = position % block_size;
  position -= fragment;
  position_running = position;
  bytes_ahead += fragment;
  blocks_ahead = (bytes_ahead + block_size - 1) / block_size;

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
   * better solution must look at the already available
   * indirect blocks (but don't call read_map!).
   */

  blocks_left = (block_t) (rip->i_size-ex64lo(position)+(block_size-1)) /
                                                                block_size;

  /* Go for the first indirect block if we are in its neighborhood. */
  ind1_pos = (EXT2_NDIR_BLOCKS) * block_size;
  if ((off_t) ex64lo(position) <= ind1_pos && rip->i_size > ind1_pos) {
	blocks_ahead++;
	blocks_left++;
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

	thisblock = read_map(rip, (off_t) ex64lo(position_running), 1);
	if (thisblock != NO_BLOCK) {
		bp = lmfs_get_block_ino(dev, thisblock, PREFETCH, rip->i_num,
			position_running);
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

  return(lmfs_get_block_ino(dev, baseblock, NORMAL, rip->i_num, position));
}


/*===========================================================================*
 *				get_dtype				     *
 *===========================================================================*/
static unsigned int get_dtype(struct ext2_disk_dir_desc *dp)
{
/* Return the type of the file identified by the given directory entry. */

  if (!HAS_INCOMPAT_FEATURE(superblock, INCOMPAT_FILETYPE))
	return DT_UNKNOWN;

  switch (dp->d_file_type) {
  case EXT2_FT_REG_FILE:	return DT_REG;
  case EXT2_FT_DIR:		return DT_DIR;
  case EXT2_FT_SYMLINK:		return DT_LNK;
  case EXT2_FT_BLKDEV:		return DT_BLK;
  case EXT2_FT_CHRDEV:		return DT_CHR;
  case EXT2_FT_FIFO:		return DT_FIFO;
  default:			return DT_UNKNOWN;
  }
}

/*===========================================================================*
 *				fs_getdents				     *
 *===========================================================================*/
ssize_t fs_getdents(ino_t ino_nr, struct fsdriver_data *data, size_t bytes,
	off_t *posp)
{
#define GETDENTS_BUFSIZE (sizeof(struct dirent) + EXT2_NAME_MAX + 1)
#define GETDENTS_ENTRIES	8
  static char getdents_buf[GETDENTS_BUFSIZE * GETDENTS_ENTRIES];
  struct fsdriver_dentry fsdentry;
  struct inode *rip;
  int r, done;
  unsigned int block_size, len;
  off_t pos, off, block_pos, new_pos, ent_pos;
  struct buf *bp;
  struct ext2_disk_dir_desc *d_desc;
  ino_t child_nr;

  /* Check whether the position is properly aligned */
  pos = *posp;
  if ((unsigned int) pos % DIR_ENTRY_ALIGN)
	return(ENOENT);

  if ((rip = get_inode(fs_dev, ino_nr)) == NULL)
	return(EINVAL);

  block_size = rip->i_sp->s_block_size;
  off = (pos % block_size);             /* Offset in block */
  block_pos = pos - off;
  done = FALSE;       /* Stop processing directory blocks when done is set */

  fsdriver_dentry_init(&fsdentry, data, bytes, getdents_buf,
	sizeof(getdents_buf));

  /* The default position for the next request is EOF. If the user's buffer
   * fills up before EOF, new_pos will be modified. */
  new_pos = rip->i_size;

  r = 0;

  for (; block_pos < rip->i_size; block_pos += block_size) {
	off_t temp_pos = block_pos;
        /* Since directories don't have holes, 'bp' cannot be NULL. */
        bp = get_block_map(rip, block_pos);     /* get a dir block */
        assert(bp != NULL);
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

		len = d_desc->d_name_len;
		assert(len <= NAME_MAX);
		assert(len <= EXT2_NAME_MAX);

		/* Need the position of this entry in the directory */
		ent_pos = block_pos + ((char *)d_desc - b_data(bp));

		child_nr = (ino_t) conv4(le_CPU, d_desc->d_ino);
		r = fsdriver_dentry_add(&fsdentry, child_nr, d_desc->d_name,
			len, get_dtype(d_desc));

		/* If the user buffer is full, or an error occurred, stop. */
		if (r <= 0) {
			done = TRUE;

			/* Record the position of this entry, it is the
			 * starting point of the next request (unless the
			 * position is modified with lseek).
			 */
			new_pos = ent_pos;
			break;
		}
	}

	put_block(bp, DIRECTORY_BLOCK);
	if (done)
		break;
  }

  if (r >= 0 && (r = fsdriver_dentry_finish(&fsdentry)) >= 0) {
	*posp = new_pos;
	rip->i_update |= ATIME;
	rip->i_dirt = IN_DIRTY;
  }

  put_inode(rip);               /* release the inode */
  return(r);
}
