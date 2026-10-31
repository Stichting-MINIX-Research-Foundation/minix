/* Block allocation at the file level (ffs_balloc), the write path, directory
 * block creation, and truncation for UFS2.
 *
 * Fragments occur only in the direct-block range: ffs_blksize() yields a full
 * block for every logical block >= UFS_NDADDR, so indirect-addressed blocks
 * (and all indirect metadata blocks) are always whole blocks.  Directories are
 * allocated one UFS_DIRBLKSIZ chunk at a time (see new_block()), so that no
 * entry ever crosses a 512-byte boundary -- required for fsck-clean images --
 * and their last block may be a fragment like any other file's.
 */

#include "fs.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/stat.h>
#include "buf.h"
#include "inode.h"
#include "super.h"

static block64_t read_indir_entry(struct inode *rip, block64_t base, int index);
static void write_indir_entry(struct inode *rip, block64_t base, int index,
	block64_t val);
static void zero_fsblock(struct inode *rip, block64_t base, int size);
static int get_indir_path(struct fs *fs, off_t lbn, int *ibp, int idx[3]);
static block64_t balloc_indir(struct inode *rip, off_t lbn);

/*===========================================================================*
 *				zero_fsblock				     *
 *===========================================================================*/
/* Zero the first 'size' bytes (numfrags worth) of the file-system block that
 * starts at fragment 'base'. */
static void zero_fsblock(struct inode *rip, block64_t base, int size)
{
  struct fs *fs = &rip->i_sp->s_fs;
  struct buf *bp;
  int i, nf;

  nf = ffs_numfrags(fs, size);
  for (i = 0; i < nf; i++) {
	bp = get_block(fs_dev, base + i, NO_READ);
	memset(b_data(bp), 0, fs->fs_fsize);
	lmfs_markdirty(bp);
	put_block(bp);
  }
}

/*===========================================================================*
 *				zero_block_range			     *
 *===========================================================================*/
/* Zero bytes [from, to) within the file-system block that starts at fragment
 * 'base', preserving any bytes before 'from'. */
static void zero_block_range(struct inode *rip, block64_t base, off_t from,
	off_t to)
{
  struct fs *fs = &rip->i_sp->s_fs;
  struct buf *bp;

  while (from < to) {
	block64_t f = base + (block64_t) (from / fs->fs_fsize);
	off_t foff = from % fs->fs_fsize;
	size_t n = fs->fs_fsize - (size_t) foff;
	if ((off_t) n > to - from)
		n = (size_t) (to - from);
	bp = get_block(fs_dev, f, NORMAL);
	memset(b_data(bp) + foff, 0, n);
	lmfs_markdirty(bp);
	put_block(bp);
	from += n;
  }
}

/*===========================================================================*
 *			read_indir_entry / write_indir_entry		     *
 *===========================================================================*/
/* An indirect block is a full fs_bsize block spanning fs_frag cache fragments;
 * the entry 'index' (32-bit on UFS1, 64-bit on UFS2) lives in exactly one of
 * them. */
static block64_t read_indir_entry(struct inode *rip, block64_t base, int index)
{
  struct fs *fs = &rip->i_sp->s_fs;
  struct buf *bp;
  off_t byteoff = (off_t) index * (off_t) FFS_DADDRSIZE(fs);
  unsigned int fragidx = (unsigned int) (byteoff / fs->fs_fsize);
  unsigned int inoff = (unsigned int) (byteoff % fs->fs_fsize);
  block64_t res;

  bp = get_block(fs_dev, base + fragidx, NORMAL);
  res = (block64_t) ffs_getdaddr(fs, b_data(bp) + inoff);
  put_block(bp);
  return(res);
}

static void write_indir_entry(struct inode *rip, block64_t base, int index,
	block64_t val)
{
  struct fs *fs = &rip->i_sp->s_fs;
  struct buf *bp;
  off_t byteoff = (off_t) index * (off_t) FFS_DADDRSIZE(fs);
  unsigned int fragidx = (unsigned int) (byteoff / fs->fs_fsize);
  unsigned int inoff = (unsigned int) (byteoff % fs->fs_fsize);

  bp = get_block(fs_dev, base + fragidx, NORMAL);
  ffs_putdaddr(fs, b_data(bp) + inoff, (int64_t) val);
  lmfs_markdirty(bp);
  put_block(bp);
}

/*===========================================================================*
 *				get_indir_path				     *
 *===========================================================================*/
/* Decompose a logical block number (>= UFS_NDADDR) into the di_ib[] index to
 * start from (*ibp) and the per-level entry indices (idx[]).  Returns the
 * number of indirection levels (1, 2 or 3). */
static int get_indir_path(struct fs *fs, off_t lbn, int *ibp, int idx[3])
{
  off_t nindir = FFS_NINDIR(fs);

  lbn -= UFS_NDADDR;
  if (lbn < nindir) {
	*ibp = 0;
	idx[0] = (int) lbn;
	return(1);
  }
  lbn -= nindir;
  if (lbn < nindir * nindir) {
	*ibp = 1;
	idx[0] = (int) (lbn / nindir);
	idx[1] = (int) (lbn % nindir);
	return(2);
  }
  lbn -= nindir * nindir;
  *ibp = 2;
  idx[0] = (int) (lbn / (nindir * nindir));
  idx[1] = (int) ((lbn / nindir) % nindir);
  idx[2] = (int) (lbn % nindir);
  return(3);
}

/*===========================================================================*
 *				balloc_indir				     *
 *===========================================================================*/
/* Allocate (if necessary) the indirect metadata blocks and the data block for
 * logical block 'lbn', which lies in the indirect range.  Returns the base
 * fragment of the data block, or NO_BLOCK with err_code set. */
static block64_t balloc_indir(struct inode *rip, off_t lbn)
{
  struct fs *fs = &rip->i_sp->s_fs;
  int idx[3], ib, num, level;
  block64_t nb, newb;

  num = get_indir_path(fs, lbn, &ib, idx);

  /* Fetch or allocate the first-level indirect block. */
  nb = (block64_t) rip->i_din.di_ib[ib];
  if (nb == NO_BLOCK) {
	newb = (block64_t) ffs_alloc(rip, lbn,
		ffs_blkpref(rip, lbn, 0, NULL), fs->fs_bsize);
	if (newb == 0) {
		err_code = ENOSPC;
		return(NO_BLOCK);
	}
	zero_fsblock(rip, newb, fs->fs_bsize);
	rip->i_din.di_ib[ib] = (int64_t) newb;
	rip->i_dirt = IN_DIRTY;
	nb = newb;
  }

  /* Walk down through the indirect blocks, allocating as needed. */
  for (level = 0; level < num - 1; level++) {
	block64_t next = read_indir_entry(rip, nb, idx[level]);
	if (next == NO_BLOCK) {
		newb = (block64_t) ffs_alloc(rip, lbn,
			ffs_blkpref(rip, lbn, 0, NULL), fs->fs_bsize);
		if (newb == 0) {
			err_code = ENOSPC;
			return(NO_BLOCK);
		}
		zero_fsblock(rip, newb, fs->fs_bsize);
		write_indir_entry(rip, nb, idx[level], newb);
		next = newb;
	}
	nb = next;
  }

  /* Finally, the data block pointer in the last indirect block. */
  newb = read_indir_entry(rip, nb, idx[num - 1]);
  if (newb == NO_BLOCK) {
	newb = (block64_t) ffs_alloc(rip, lbn,
		ffs_blkpref(rip, lbn, 0, NULL), fs->fs_bsize);
	if (newb == 0) {
		err_code = ENOSPC;
		return(NO_BLOCK);
	}
	zero_fsblock(rip, newb, fs->fs_bsize);
	write_indir_entry(rip, nb, idx[num - 1], newb);
  }

  return(newb);
}

/*===========================================================================*
 *				ffs_balloc				     *
 *===========================================================================*/
block64_t ffs_balloc(struct inode *rip, off_t off, int size)
{
/* Ensure that the file-system block holding byte offset 'off' is allocated and
 * large enough to hold 'size' bytes written at 'off'.  Returns the base
 * fragment of that block, or NO_BLOCK with err_code set on failure.  Fragments
 * in the direct range are grown (and possibly relocated) as the file grows.
 */
  struct fs *fs = &rip->i_sp->s_fs;
  off_t lbn, lastlbn;
  block64_t nb, newb;
  int osize, nsize;

  err_code = OK;
  lbn = ffs_lblkno(fs, off);
  size = (int) ffs_blkoff(fs, off) + size;
  assert(size <= fs->fs_bsize);

  /* If the file currently ends in a fragment and we are extending it into a
   * later block, that fragment must first be rounded up to a full block. */
  lastlbn = ffs_lblkno(fs, rip->i_din.di_size);
  if (lastlbn < UFS_NDADDR && lastlbn < lbn &&
      (block64_t) rip->i_din.di_db[lastlbn] != NO_BLOCK) {
	osize = (int) ffs_blksize(fs, rip->i_din.di_size, lastlbn);
	if (osize < fs->fs_bsize && osize > 0) {
		newb = (block64_t) ffs_realloccg(rip, lastlbn,
			(daddr_t) rip->i_din.di_db[lastlbn],
			ffs_blkpref(rip, lastlbn, (int) lastlbn,
			    rip->i_din.di_db),
			osize, fs->fs_bsize);
		if (newb == 0) {
			err_code = ENOSPC;
			return(NO_BLOCK);
		}
		rip->i_din.di_db[lastlbn] = (int64_t) newb;
		/* Zero the bytes between the old EOF and the new block
		 * boundary: realloccg does not clear the frags it grows into,
		 * and they now fall inside the file's size. */
		zero_block_range(rip, newb,
			(off_t) ffs_blkoff(fs, rip->i_din.di_size),
			(off_t) fs->fs_bsize);
		rip->i_din.di_size = ffs_lblktosize(fs, lastlbn + 1);
		rip->i_dirt = IN_DIRTY;
	}
  }

  if (lbn < UFS_NDADDR) {
	nb = (block64_t) rip->i_din.di_db[lbn];

	if (nb != NO_BLOCK &&
	    (u_int64_t) rip->i_din.di_size >= ffs_lblktosize(fs, lbn + 1)) {
		/* An existing, already-full direct block. */
		return(nb);
	}

	if (nb != NO_BLOCK) {
		/* An existing fragment; grow it if the request is larger. */
		osize = (int) ffs_fragroundup(fs,
			ffs_blkoff(fs, rip->i_din.di_size));
		nsize = (int) ffs_fragroundup(fs, size);
		if (nsize <= osize)
			return(nb);
		newb = (block64_t) ffs_realloccg(rip, lbn, (daddr_t) nb,
			ffs_blkpref(rip, lbn, (int) lbn, rip->i_din.di_db),
			osize, nsize);
		if (newb == 0) {
			err_code = ENOSPC;
			return(NO_BLOCK);
		}
		rip->i_din.di_db[lbn] = (int64_t) newb;
		/* Zero the grown region: realloccg leaves the new frags with
		 * stale contents, and a seek-past-EOF write may leave a hole
		 * below the write offset that must read as zero. */
		zero_block_range(rip, newb,
			(off_t) ffs_blkoff(fs, rip->i_din.di_size), (off_t) nsize);
		rip->i_dirt = IN_DIRTY;
		return(newb);
	}

	/* The block is not yet allocated. */
	if ((u_int64_t) rip->i_din.di_size < ffs_lblktosize(fs, lbn + 1))
		nsize = (int) ffs_fragroundup(fs, size);
	else
		nsize = fs->fs_bsize;
	newb = (block64_t) ffs_alloc(rip, lbn,
		ffs_blkpref(rip, lbn, (int) lbn, rip->i_din.di_db), nsize);
	if (newb == 0) {
		err_code = ENOSPC;
		return(NO_BLOCK);
	}
	zero_fsblock(rip, newb, nsize);
	rip->i_din.di_db[lbn] = (int64_t) newb;
	rip->i_dirt = IN_DIRTY;
	return(newb);
  }

  /* Indirect range: always full blocks. */
  return(balloc_indir(rip, lbn));
}

/*===========================================================================*
 *				new_block				     *
 *===========================================================================*/
struct buf *new_block(struct inode *rip, off_t position)
{
/* Allocate and initialize a fresh UFS_DIRBLKSIZ directory chunk at the (chunk-
 * aligned) byte offset 'position', as one empty entry spanning it.  Returns the
 * cache buffer holding that chunk -- which starts 'position' modulo the cache
 * block size into the buffer, not necessarily at its beginning -- or NULL with
 * err_code set.
 *
 * A directory grows one chunk at a time rather than one file-system block at a
 * time, because that is how the file systems this server is handed are laid
 * out: makefs(8) and newfs(8) both leave a directory's last block partial (a
 * fresh directory is one chunk), so a directory's size is a multiple of
 * UFS_DIRBLKSIZ and not of fs_bsize.  It is also how NetBSD's ufs_direnter()
 * extends one.  Asking ffs_balloc() for a whole block at an offset that is not
 * block-aligned cannot work at all: it sizes the request from the offset within
 * the block, so the request would run past the end of that block.
 */
  struct buf *bp;
  struct direct *dp;
  unsigned int off;

  assert(position % UFS_DIRBLKSIZ == 0);

  if (ffs_balloc(rip, position, UFS_DIRBLKSIZ) == NO_BLOCK)
	return(NULL);

  /* ffs_balloc() zeroed whatever it added, and the rest of this cache block
   * holds earlier chunks of the same directory, so read it as it is. */
  if ((bp = get_block_map(rip, (uint64_t) position)) == NULL) {
	err_code = EIO;
	return(NULL);
  }

  off = (unsigned int) (position % rip->i_sp->s_block_size);
  dp = (struct direct *) (b_data(bp) + off);
  memset(dp, 0, UFS_DIRBLKSIZ);
  dp->d_reclen = UFS_DIRBLKSIZ;
  lmfs_markdirty(bp);

  return(bp);
}

/*===========================================================================*
 *				zero_block				     *
 *===========================================================================*/
void zero_block(struct buf *bp)
{
/* Zero an entire cache buffer (one fragment). */
  struct fs *fs = &superblock->s_fs;

  memset(b_data(bp), 0, fs->fs_fsize);
  lmfs_markdirty(bp);
}

/*===========================================================================*
 *			indirect-block truncation			     *
 *===========================================================================*/
/* Number of logical file blocks addressed by one entry of an indirect block at
 * the given level (0 == single indirect -> 1 data block per entry). */
static off_t indir_subspan(struct fs *fs, int level)
{
  off_t span = 1;
  int i;

  for (i = 0; i < level; i++)
	span *= FFS_NINDIR(fs);
  return(span);
}

/* Recursively free, within the indirect block at fragment 'ind' (level: 0 ==
 * single indirect), every entry whose covered logical file blocks lie entirely
 * beyond 'lastkeep'.  'first' is the absolute logical block number addressed by
 * entry 0.  Entries straddling 'lastkeep' are descended into but kept; the
 * indirect block 'ind' itself is freed by the caller when appropriate.
 */
static void indir_free(struct inode *rip, block64_t ind, int level, off_t first,
	off_t lastkeep)
{
  struct fs *fs = &rip->i_sp->s_fs;
  off_t subspan = indir_subspan(fs, level);
  int nindir = FFS_NINDIR(fs);
  int e;
  block64_t nb;

  for (e = 0; e < nindir; e++) {
	off_t entry_first = first + (off_t) e * subspan;
	off_t entry_last = entry_first + subspan - 1;

	if (entry_first > lastkeep) {
		/* Whole sub-tree is gone. */
		nb = read_indir_entry(rip, ind, e);
		if (nb == NO_BLOCK)
			continue;
		if (level > 0)
			indir_free(rip, nb, level - 1, entry_first, lastkeep);
		ffs_blkfree(rip, (daddr_t) nb, fs->fs_bsize);
		write_indir_entry(rip, ind, e, NO_BLOCK);
	} else if (entry_last > lastkeep && level > 0) {
		/* Boundary entry: descend but keep it. */
		nb = read_indir_entry(rip, ind, e);
		if (nb != NO_BLOCK)
			indir_free(rip, nb, level - 1, entry_first, lastkeep);
	}
  }
}

/*
 * Zero the bytes in [start, end) that are backed by allocated blocks, leaving
 * holes as they are.  Used both to punch a range and to clear what is left of
 * the block that survives a truncation.
 */
static void zero_range(struct inode *rip, off_t start, off_t end)
{
  struct fs *fs = &rip->i_sp->s_fs;
  off_t pos = start;

  while (pos < end) {
	block64_t base = read_map(rip, pos, 0);
	off_t boff = pos % fs->fs_fsize;
	size_t n = fs->fs_fsize - boff;

	if ((off_t) n > end - pos)
		n = (size_t) (end - pos);
	if (base != NO_BLOCK) {
		struct buf *bp = get_block(fs_dev, base, NORMAL);

		memset(b_data(bp) + boff, 0, n);
		lmfs_markdirty(bp);
		put_block(bp);
	}
	pos += n;
  }
}

/*===========================================================================*
 *				truncate_inode				     *
 *===========================================================================*/
int truncate_inode(struct inode *rip, off_t length)
{
/* Set the file's size to 'length', allocating or freeing blocks as needed. */
  struct fs *fs = &rip->i_sp->s_fs;
  off_t osize, lbn, lastblock;
  block64_t bn;
  int i, level;
  int oldspace, newspace;

  osize = (off_t) rip->i_din.di_size;

  /* Specials and fifos keep size 0. */
  if (S_ISCHR(rip->i_din.di_mode) || S_ISBLK(rip->i_din.di_mode) ||
      S_ISFIFO(rip->i_din.di_mode) || S_ISSOCK(rip->i_din.di_mode))
	return(OK);

  /*
   * An inline symlink has its target in the block-pointer area and owns no
   * blocks at all, so there is nothing to free: walking di_db[]/di_ib[] below
   * would free the target's own bytes as if they were block numbers, which
   * lands on whatever the text happens to spell.  The only length this can be
   * asked for is 0, from the last unlink; NetBSD's ffs_truncate() treats the
   * case the same way, by clearing the target and the size.
   */
  if (inline_symlink(rip)) {
	if (length != 0)
		return(EINVAL);
	memset(rip->i_din.di_db, 0, sizeof(rip->i_din.di_db));
	memset(rip->i_din.di_ib, 0, sizeof(rip->i_din.di_ib));
	rip->i_din.di_size = 0;
	rip->i_update |= CTIME | MTIME;
	rip->i_dirt = IN_DIRTY;
	return(OK);
  }

  if (length < 0)
	return(EINVAL);
  if ((u_int64_t) length > (u_int64_t) rip->i_sp->s_max_size)
	return(EFBIG);

  if (length == osize) {
	rip->i_update |= CTIME | MTIME;
	rip->i_dirt = IN_DIRTY;
	return(OK);
  }

  /*
   * Growing the file.  The new EOF must end up backed by a correctly sized
   * block/fragment so that the on-disk fragment is consistent with di_size:
   * later ffs_blksize()/ffs_balloc() calls derive the EOF fragment size from
   * di_size, so advancing di_size past a too-small fragment corrupts the
   * fragment bookkeeping the next time that block is grown.  The interior of a
   * sparse growth is left as a hole (reads as zero).
   */
  if (length > osize) {
	if (length > 0) {
		off_t newlast = ffs_lblkno(fs, length - 1);

		if (newlast < UFS_NDADDR) {
			/* The new EOF lands in the direct range: allocate and
			 * size the EOF fragment.  ffs_balloc also rounds any
			 * earlier partial direct block up to a full block and
			 * zeroes the bytes it grows into. */
			(void) ffs_balloc(rip, length - 1, 1);
			if (err_code != OK)
				return(err_code);
		} else if (osize > 0 && ffs_lblkno(fs, osize) < UFS_NDADDR &&
		    ffs_blkoff(fs, osize) != 0) {
			/* The new EOF is in the indirect range (always full
			 * blocks), but the old EOF was a direct fragment that
			 * must now be rounded up to a full block. */
			off_t eob = ffs_blkroundup(fs, osize);
			(void) ffs_balloc(rip, eob - 1, 1);
			if (err_code != OK)
				return(err_code);
		}
	}
	rip->i_din.di_size = length;

	/*
	 * Zero the stale bytes between the old end of file and the end of the
	 * block that held it.  Those bytes are allocated but were never part
	 * of the file, and growing past them must show zeroes rather than
	 * whatever the block last held -- which, with blocks coming straight
	 * off the free list, is often the file that was just deleted.  Blocks
	 * past that one are holes and read as zero by themselves, so the range
	 * stops at the old end-of-file block; zero_range() skips anything
	 * unallocated within it anyway.  NetBSD gets this from
	 * ufs_balloc_range(), which zero-fills what it extends into.
	 */
	if (osize > 0 && ffs_blkoff(fs, osize) != 0) {
		off_t oldlbn = ffs_lblkno(fs, osize);
		off_t zend = (off_t) ffs_lblktosize(fs, oldlbn + 1);

		if (zend > length)
			zend = length;
		if (zend > osize)
			zero_range(rip, osize, zend);
	}

	rip->i_update |= CTIME | MTIME;
	rip->i_dirt = IN_DIRTY;
	return(OK);
  }

  /*
   * Shrinking the file.  First zero what is left of the block that keeps the
   * new end of file, from that end to wherever the block stops.  Those bytes
   * are still allocated and still reachable: a truncation back up to the old
   * size, or a write past the new end, has to find zeroes there rather than
   * what the file used to hold.  Freeing whole blocks below is not enough --
   * the block holding the new end is not freed.  NetBSD's ffs_truncate() does
   * the same with ubc_zerorange(), also before it frees anything, and test50
   * is what notices when it is missing.
   *
   * This reads the old size, so it has to happen before di_size is committed.
   */
  if (S_ISREG(rip->i_din.di_mode) && ffs_blkoff(fs, length) != 0) {
	off_t eoflbn = ffs_lblkno(fs, length);
	off_t eoz = (off_t) ffs_lblktosize(fs, eoflbn) +
	    (off_t) ffs_blksize(fs, osize, eoflbn);

	if (eoz > osize)
		eoz = osize;
	if (eoz > length)
		zero_range(rip, length, eoz);
  }

  /* Commit the new size, then free everything that is no longer reachable. */
  rip->i_din.di_size = length;

  /* lastblock is the highest logical block to keep (-1 if length == 0). */
  lastblock = ffs_lblkno(fs, length + fs->fs_bsize - 1) - 1;

  /* Free indirect trees, from the triple- down to the single-indirect.  Each
   * di_ib[level] addresses logical blocks starting at firstib(level). */
  for (level = 2; level >= 0; level--) {
	off_t nindir = FFS_NINDIR(fs);
	off_t firstib = UFS_NDADDR;
	off_t treespan;

	if (level >= 1)
		firstib += nindir;
	if (level >= 2)
		firstib += nindir * nindir;
	treespan = indir_subspan(fs, level + 1);	/* nindir^(level+1) */

	bn = (block64_t) rip->i_din.di_ib[level];
	if (bn == NO_BLOCK)
		continue;

	if (lastblock < firstib) {
		/* The whole tree, including the top indirect block, is gone. */
		indir_free(rip, bn, level, firstib, lastblock);
		ffs_blkfree(rip, (daddr_t) bn, fs->fs_bsize);
		rip->i_din.di_ib[level] = NO_BLOCK;
	} else if (lastblock < firstib + treespan - 1) {
		/* Partial truncation: free the tail entries, keep the block. */
		indir_free(rip, bn, level, firstib, lastblock);
	}
	/* else: the entire tree is still in use. */
  }

  /* Free direct blocks beyond the new size.  Compare as off_t: lastblock can
   * exceed INT_MAX for multi-terabyte files. */
  for (i = UFS_NDADDR - 1; (off_t) i > lastblock; i--) {
	bn = (block64_t) rip->i_din.di_db[i];
	if (bn == NO_BLOCK)
		continue;
	oldspace = (int) ffs_blksize(fs, osize, i);
	ffs_blkfree(rip, (daddr_t) bn, oldspace);
	rip->i_din.di_db[i] = NO_BLOCK;
  }

  /*
   * If the new last block is a direct-range fragment that shrank, free the
   * trailing fragments that are no longer needed.
   */
  lbn = ffs_lblkno(fs, length);
  if (length != 0 && lbn < UFS_NDADDR &&
      (bn = (block64_t) rip->i_din.di_db[lbn]) != NO_BLOCK) {
	oldspace = (int) ffs_blksize(fs, osize, lbn);
	newspace = (int) ffs_blksize(fs, length, lbn);
	if (newspace < oldspace) {
		block64_t relfrag = bn + (block64_t) ffs_numfrags(fs, newspace);
		ffs_blkfree(rip, (daddr_t) relfrag, oldspace - newspace);
	}
  }

  /*
   * If the new end of a regular file landed inside a hole, materialize the
   * EOF fragment.  ffs_balloc() and this function derive the physical size
   * of the EOF block from di_size, so leaving the EOF block unallocated
   * would make a later write to it under-allocate and a later truncate
   * over-free its fragments (double-frees corrupting cs_nffree).  NetBSD's
   * ffs_truncate() maintains the same invariant with ufs_balloc_range().
   */
  if (length != 0 && lbn < UFS_NDADDR && S_ISREG(rip->i_din.di_mode) &&
      ffs_blkoff(fs, length) != 0 &&
      (block64_t) rip->i_din.di_db[lbn] == NO_BLOCK) {
	(void) ffs_balloc(rip, length - 1, 1);
	if (err_code != OK)
		return(err_code);
  }

  rip->i_update |= CTIME | MTIME;
  rip->i_dirt = IN_DIRTY;
  return(OK);
}

/*===========================================================================*
 *				fs_trunc				     *
 *===========================================================================*/
int fs_trunc(ino_t ino_nr, off_t start, off_t end)
{
/* Truncate a file to 'start' (when end == 0), or punch a hole in the range
 * [start, end) by zeroing it (a minimal implementation that does not free
 * whole blocks in the interior). */
  struct inode *rip;

  if ((rip = find_inode(fs_dev, ino_nr)) == NULL)
	return(EINVAL);

  if (end == 0)
	return(truncate_inode(rip, start));

  /* Range punch: zero the affected bytes in place. */
  zero_range(rip, start, end);

  rip->i_update |= CTIME | MTIME;
  rip->i_dirt = IN_DIRTY;

  return(OK);
}
