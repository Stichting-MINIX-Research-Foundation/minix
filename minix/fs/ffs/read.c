/* This file contains the block-mapping (bmap), read, and getdents logic.
 *
 * The cache operates in fragment-sized blocks (sp->s_block_size == fs_fsize).
 * A UFS file-system block of fs_bsize bytes therefore occupies fs_frag
 * consecutive cache blocks, and all on-disk addresses (di_db[], di_ib[] and
 * indirect-block entries) are fragment numbers that map straight onto cache
 * block numbers.
 */

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

static int rw_chunk(struct inode *rip, off_t position, unsigned off,
	size_t chunk, int call, struct fsdriver_data *data, unsigned buf_off,
	unsigned int block_size);
static block64_t indir_get(struct inode *rip, block64_t indir, off_t index,
	int iomode);
static block64_t bmap_lbn(struct inode *rip, off_t lbn, int iomode);

/*===========================================================================*
 *				fs_readwrite				     *
 *===========================================================================*/
ssize_t fs_readwrite(ino_t ino_nr, struct fsdriver_data *data, size_t nrbytes,
	off_t position, int call)
{
  struct inode *rip;
  off_t f_size, bytes_left;
  size_t off, cum_io, block_size, chunk;
  int r;

  if ((rip = find_inode(fs_dev, ino_nr)) == NULL)
	return(EINVAL);

  if (call == FSC_WRITE && rip->i_sp->s_rd_only)
	return(EROFS);

  block_size = rip->i_sp->s_block_size;
  f_size = (off_t) rip->i_din.di_size;

  /* A write may not grow the file beyond the maximum representable size. */
  if (call == FSC_WRITE &&
      position > (off_t) (rip->i_sp->s_max_size - nrbytes))
	return(EFBIG);

  r = OK;
  cum_io = 0;

  /* Split the transfer into chunks that don't span two cache blocks. */
  while (nrbytes > 0) {
	off = (size_t) (position % block_size);	/* offset in block */
	chunk = block_size - off;
	if (chunk > nrbytes)
		chunk = nrbytes;

	if (call == FSC_READ) {
		if (position >= f_size)
			break;			/* we are beyond EOF */
		bytes_left = f_size - position;
		if ((off_t) chunk > bytes_left)
			chunk = (size_t) bytes_left;
	}

	r = rw_chunk(rip, position, off, chunk, call, data, cum_io, block_size);
	if (r != OK)
		break;

	nrbytes -= chunk;
	cum_io += chunk;
	position += (off_t) chunk;

	/* Grow the file as we go, so ffs_balloc sees a consistent size when
	 * deciding whether to round the previous fragment up to a block. */
	if (call == FSC_WRITE && position > (off_t) rip->i_din.di_size) {
		rip->i_din.di_size = position;
		f_size = position;
	}
  }

  rip->i_seek = NO_SEEK;

  if (r != OK)
	return(r);

  if (call == FSC_READ)
	rip->i_update |= ATIME;	/* read-only fs: update_times() is a no-op */
  if (call == FSC_WRITE) {
	rip->i_update |= CTIME | MTIME;
	rip->i_dirt = IN_DIRTY;
  }

  return((ssize_t) cum_io);
}

/*===========================================================================*
 *				rw_chunk				     *
 *===========================================================================*/
static int rw_chunk(struct inode *rip, off_t position, unsigned off,
	size_t chunk, int call, struct fsdriver_data *data, unsigned buf_off,
	unsigned int block_size)
{
/* Read, write or peek (part of) a single cache block (fragment). */
  struct fs *fs = &rip->i_sp->s_fs;
  struct buf *bp;
  block64_t b;
  dev_t dev = rip->i_dev;
  ino_t ino = rip->i_num;
  uint64_t ino_off = rounddown(position, block_size);
  int r, getmode;
  static char wbuf[65536];	/* single-threaded server; >= max fs_fsize */

  if (call == FSC_WRITE) {
	block64_t base;

	/* Fetch the user data BEFORE allocating.  The copy can fail with
	 * EFAULT as part of the normal VM grant-fault protocol (VFS resolves
	 * the fault and retries the whole call); if that happened after
	 * ffs_balloc had extended the file, the inode would be left with a
	 * block allocated beyond di_size, which a later ffs_balloc would
	 * mis-size (fragment bookkeeping corruption and double frees).
	 */
	assert(chunk <= sizeof(wbuf));
	if ((r = fsdriver_copyin(data, buf_off, wbuf, chunk)) != OK)
		return(r);

	/* Make sure the block holding this position is allocated (and large
	 * enough); ffs_balloc zeroes freshly allocated blocks. */
	base = ffs_balloc(rip, position, (int) chunk);
	if (base == NO_BLOCK)
		return(err_code);
	b = base + (block64_t) (ffs_blkoff(fs, position) >> fs->fs_fshift);
  } else {
	b = read_map(rip, position, 0);
  }

  if (b == NO_BLOCK) {
	if (call == FSC_READ) {
		/* Reading from a hole: deliver zeros. */
		return fsdriver_zero(data, buf_off, chunk);
	} else {
		/* Peeking a hole: report it to VM. */
		lmfs_zero_block_ino(dev, ino, ino_off);
		return(OK);
	}
  }

  /* A full-block overwrite need not be read in first. */
  getmode = (call == FSC_WRITE && chunk == block_size) ? NO_READ : NORMAL;

  if ((r = lmfs_get_block_ino(&bp, dev, b, getmode, ino, ino_off)) != OK)
	panic("ffs: error getting block (%llu,%llu): %d",
	    (unsigned long long)dev, (unsigned long long)b, r);

  if (call == FSC_READ) {
	r = fsdriver_copyout(data, buf_off, b_data(bp) + off, chunk);
  } else if (call == FSC_WRITE) {
	memcpy(b_data(bp) + off, wbuf, chunk);
	lmfs_markdirty(bp);
	r = OK;
  } else {
	r = OK;			/* FSC_PEEK: block is now resident in the cache */
  }

  put_block(bp);

  return(r);
}

/*===========================================================================*
 *				indir_get				     *
 *===========================================================================*/
static block64_t indir_get(struct inode *rip, block64_t indir, off_t index,
	int iomode)
{
/* Return entry 'index' of the indirect block that starts at fragment 'indir'.
 * An indirect block is fs_bsize bytes and spans fs_frag cache fragments; the
 * wanted entry (32-bit on UFS1, 64-bit on UFS2) lives in exactly one of them.
 */
  struct fs *fs = &rip->i_sp->s_fs;
  struct buf *bp;
  off_t byteoff;
  unsigned int fragidx, inoff;
  block64_t res;

  if (indir == NO_BLOCK)
	return(NO_BLOCK);

  byteoff = index * (off_t) FFS_DADDRSIZE(fs);
  fragidx = (unsigned int) (byteoff / fs->fs_fsize);
  inoff = (unsigned int) (byteoff % fs->fs_fsize);

  bp = get_block(rip->i_dev, indir + fragidx, iomode);
  if (bp == NULL)
	return(NO_BLOCK);	/* PEEK miss */

  res = (block64_t) ffs_getdaddr(fs, b_data(bp) + inoff);
  put_block(bp);

  return(res);
}

/*===========================================================================*
 *				bmap_lbn				     *
 *===========================================================================*/
static block64_t bmap_lbn(struct inode *rip, off_t lbn, int iomode)
{
/* Map a logical (full-block) file block number to the fragment number at which
 * that block starts, following the direct and single/double/triple indirect
 * block pointers.  Returns NO_BLOCK for a hole.
 */
  struct fs *fs = &rip->i_sp->s_fs;
  off_t nindir = FFS_NINDIR(fs);
  block64_t b;

  if (lbn < UFS_NDADDR)
	return (block64_t) rip->i_din.di_db[lbn];

  lbn -= UFS_NDADDR;
  if (lbn < nindir)
	return indir_get(rip, (block64_t) rip->i_din.di_ib[0], lbn, iomode);

  lbn -= nindir;
  if (lbn < nindir * nindir) {
	b = indir_get(rip, (block64_t) rip->i_din.di_ib[1], lbn / nindir,
		iomode);
	return indir_get(rip, b, lbn % nindir, iomode);
  }

  lbn -= nindir * nindir;
  /* Triple indirect.  Files large enough to exceed this are beyond the
   * maximum representable UFS2 file size, so no further range check is needed.
   */
  b = indir_get(rip, (block64_t) rip->i_din.di_ib[2],
	lbn / (nindir * nindir), iomode);
  b = indir_get(rip, b, (lbn / nindir) % nindir, iomode);
  return indir_get(rip, b, lbn % nindir, iomode);
}

/*===========================================================================*
 *				read_map				     *
 *===========================================================================*/
block64_t read_map(struct inode *rip, off_t position, int opportunistic)
{
/* Given an inode and a byte position within the file, return the device
 * fragment (cache block) number that holds that position, or NO_BLOCK.
 */
  struct fs *fs = &rip->i_sp->s_fs;
  off_t lbn;
  block64_t base, fragsel;
  int iomode = opportunistic ? PEEK : NORMAL;

  lbn = ffs_lblkno(fs, position);
  fragsel = (block64_t) (ffs_blkoff(fs, position) >> fs->fs_fshift);

  base = bmap_lbn(rip, lbn, iomode);
  if (base == NO_BLOCK)
	return(NO_BLOCK);

  return(base + fragsel);
}

/*===========================================================================*
 *				get_block_map				     *
 *===========================================================================*/
struct buf *get_block_map(struct inode *rip, uint64_t position)
{
/* Return the cache buffer holding the (block-aligned) file position, or NULL
 * for a hole.  Used to walk directory blocks, which never contain holes.
 */
  struct buf *bp;
  block64_t b;
  unsigned int block_size;
  int r;

  b = read_map(rip, (off_t) position, 0);
  if (b == NO_BLOCK)
	return(NULL);

  block_size = rip->i_sp->s_block_size;
  position = rounddown(position, block_size);

  if ((r = lmfs_get_block_ino(&bp, rip->i_dev, b, NORMAL, rip->i_num,
      position)) != OK)
	panic("ffs: error getting block (%llu,%llu): %d",
	    (unsigned long long)rip->i_dev, (unsigned long long)b, r);

  return(bp);
}

/*===========================================================================*
 *				fs_getdents				     *
 *===========================================================================*/
ssize_t fs_getdents(ino_t ino_nr, struct fsdriver_data *data, size_t bytes,
	off_t *posp)
{
#define GETDENTS_BUFSIZE (sizeof(struct dirent) + FFS_MAXNAMLEN + 1)
#define GETDENTS_ENTRIES 8
  static char getdents_buf[GETDENTS_BUFSIZE * GETDENTS_ENTRIES];
  struct fsdriver_dentry fsdentry;
  struct inode *rip;
  struct buf *bp;
  struct direct *dp;
  off_t pos, chunk_pos, new_pos, ent_pos, dir_size;
  unsigned int block_size, len, reclen, coff;
  int r, done;

  /* Directory entries are aligned to 4 bytes. */
  pos = *posp;
  if (pos % 4)
	return(ENOENT);

  if ((rip = get_inode(fs_dev, ino_nr)) == NULL)
	return(EINVAL);

  block_size = rip->i_sp->s_block_size;
  dir_size = (off_t) rip->i_din.di_size;
  done = FALSE;

  fsdriver_dentry_init(&fsdentry, data, bytes, getdents_buf,
	sizeof(getdents_buf));

  /* The default next position is EOF; lowered below if the buffer fills up. */
  new_pos = dir_size;
  r = 0;

  /* Walk the directory one UFS_DIRBLKSIZ chunk at a time: entries never cross
   * a chunk boundary. */
  for (chunk_pos = rounddown(pos, UFS_DIRBLKSIZ); chunk_pos < dir_size;
       chunk_pos += UFS_DIRBLKSIZ) {
	/* Directories have no holes, so the block is always present. */
	bp = get_block_map(rip, (uint64_t) chunk_pos);
	assert(bp != NULL);

	for (coff = 0; coff < UFS_DIRBLKSIZ; coff += reclen) {
		dp = (struct direct *) (b_data(bp) +
			(chunk_pos % block_size) + coff);
		reclen = dp->d_reclen;

		/* Guard against a corrupt zero-length record. */
		if (reclen < UFS_DIRECTSIZ(0)) {
			done = TRUE;
			break;
		}

		ent_pos = chunk_pos + coff;
		if (ent_pos + (off_t) reclen <= pos)
			continue;	/* skip entries before the request pos */

		if (dp->d_ino == 0)
			continue;	/* entry not in use */

		len = dp->d_namlen;
		assert(len <= FFS_MAXNAMLEN);

		r = fsdriver_dentry_add(&fsdentry, (ino_t) dp->d_ino,
			dp->d_name, len, dp->d_type);

		/* If the user buffer is full, or an error occurred, stop. */
		if (r <= 0) {
			done = TRUE;
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
	rip->i_update |= ATIME;
  }

  put_inode(rip);
  return(r);
}
