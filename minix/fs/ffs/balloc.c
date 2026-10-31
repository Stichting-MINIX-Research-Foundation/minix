/* Cylinder-group block and fragment allocator for UFS2.
 *
 * This is a self-contained port of the on-disk allocation semantics from
 * NetBSD's sys/ufs/ffs/ffs_alloc.c, adapted to MINIX: the kernel buffer/lock/
 * snapshot/quota/cluster machinery is dropped, and cylinder groups are read
 * into and written back from a contiguous bounce buffer assembled out of the
 * fragment-sized cache blocks (see CLAUDE.md, "fragment as cache block").
 *
 * The on-disk fragment bookkeeping (cg_frsum, cg_cs, fs_cstotal) is maintained
 * exactly as the original FFS does, using the fragtbl/around/inside tables, so
 * that images stay fsck-clean.
 */

#include "fs.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "buf.h"
#include "inode.h"
#include "super.h"

/* NetBSD spells this 'numfrags'; the on-disk header exports ffs_numfrags. */
#define numfrags(fs, size)	ffs_numfrags((fs), (size))

/* Bit patterns identifying fragments in the block map, used as
 * ((map & around[siz]) == inside[siz]).  Copied from ffs_tables.c.
 */
static const int around[9] = {
	0x3, 0x7, 0xf, 0x1f, 0x3f, 0x7f, 0xff, 0x1ff, 0x3ff
};
static const int inside[9] = {
	0x0, 0x2, 0x6, 0xe, 0x1e, 0x3e, 0x7e, 0xfe, 0x1fe
};

/* fragtbl[frag][map] tells whether a fragment of a given size is available in
 * a block-map byte: a run of 'siz' free frags exists iff
 * (1 << (siz-1)) & fragtbl[fs_frag][map].  Copied from ffs_tables.c.
 */
static const u_char fragtbl124[256] = {
	0x00, 0x16, 0x16, 0x2a, 0x16, 0x16, 0x26, 0x4e,
	0x16, 0x16, 0x16, 0x3e, 0x2a, 0x3e, 0x4e, 0x8a,
	0x16, 0x16, 0x16, 0x3e, 0x16, 0x16, 0x36, 0x5e,
	0x16, 0x16, 0x16, 0x3e, 0x3e, 0x3e, 0x5e, 0x9e,
	0x16, 0x16, 0x16, 0x3e, 0x16, 0x16, 0x36, 0x5e,
	0x16, 0x16, 0x16, 0x3e, 0x3e, 0x3e, 0x5e, 0x9e,
	0x2a, 0x3e, 0x3e, 0x2a, 0x3e, 0x3e, 0x2e, 0x6e,
	0x3e, 0x3e, 0x3e, 0x3e, 0x2a, 0x3e, 0x6e, 0xaa,
	0x16, 0x16, 0x16, 0x3e, 0x16, 0x16, 0x36, 0x5e,
	0x16, 0x16, 0x16, 0x3e, 0x3e, 0x3e, 0x5e, 0x9e,
	0x16, 0x16, 0x16, 0x3e, 0x16, 0x16, 0x36, 0x5e,
	0x16, 0x16, 0x16, 0x3e, 0x3e, 0x3e, 0x5e, 0x9e,
	0x26, 0x36, 0x36, 0x2e, 0x36, 0x36, 0x26, 0x6e,
	0x36, 0x36, 0x36, 0x3e, 0x2e, 0x3e, 0x6e, 0xae,
	0x4e, 0x5e, 0x5e, 0x6e, 0x5e, 0x5e, 0x6e, 0x4e,
	0x5e, 0x5e, 0x5e, 0x7e, 0x6e, 0x7e, 0x4e, 0xce,
	0x16, 0x16, 0x16, 0x3e, 0x16, 0x16, 0x36, 0x5e,
	0x16, 0x16, 0x16, 0x3e, 0x3e, 0x3e, 0x5e, 0x9e,
	0x16, 0x16, 0x16, 0x3e, 0x16, 0x16, 0x36, 0x5e,
	0x16, 0x16, 0x16, 0x3e, 0x3e, 0x3e, 0x5e, 0x9e,
	0x16, 0x16, 0x16, 0x3e, 0x16, 0x16, 0x36, 0x5e,
	0x16, 0x16, 0x16, 0x3e, 0x3e, 0x3e, 0x5e, 0x9e,
	0x3e, 0x3e, 0x3e, 0x3e, 0x3e, 0x3e, 0x3e, 0x7e,
	0x3e, 0x3e, 0x3e, 0x3e, 0x3e, 0x3e, 0x7e, 0xbe,
	0x2a, 0x3e, 0x3e, 0x2a, 0x3e, 0x3e, 0x2e, 0x6e,
	0x3e, 0x3e, 0x3e, 0x3e, 0x2a, 0x3e, 0x6e, 0xaa,
	0x3e, 0x3e, 0x3e, 0x3e, 0x3e, 0x3e, 0x3e, 0x7e,
	0x3e, 0x3e, 0x3e, 0x3e, 0x3e, 0x3e, 0x7e, 0xbe,
	0x4e, 0x5e, 0x5e, 0x6e, 0x5e, 0x5e, 0x6e, 0x4e,
	0x5e, 0x5e, 0x5e, 0x7e, 0x6e, 0x7e, 0x4e, 0xce,
	0x8a, 0x9e, 0x9e, 0xaa, 0x9e, 0x9e, 0xae, 0xce,
	0x9e, 0x9e, 0x9e, 0xbe, 0xaa, 0xbe, 0xce, 0x8a,
};

static const u_char fragtbl8[256] = {
	0x00, 0x01, 0x01, 0x02, 0x01, 0x01, 0x02, 0x04,
	0x01, 0x01, 0x01, 0x03, 0x02, 0x03, 0x04, 0x08,
	0x01, 0x01, 0x01, 0x03, 0x01, 0x01, 0x03, 0x05,
	0x02, 0x03, 0x03, 0x02, 0x04, 0x05, 0x08, 0x10,
	0x01, 0x01, 0x01, 0x03, 0x01, 0x01, 0x03, 0x05,
	0x01, 0x01, 0x01, 0x03, 0x03, 0x03, 0x05, 0x09,
	0x02, 0x03, 0x03, 0x02, 0x03, 0x03, 0x02, 0x06,
	0x04, 0x05, 0x05, 0x06, 0x08, 0x09, 0x10, 0x20,
	0x01, 0x01, 0x01, 0x03, 0x01, 0x01, 0x03, 0x05,
	0x01, 0x01, 0x01, 0x03, 0x03, 0x03, 0x05, 0x09,
	0x01, 0x01, 0x01, 0x03, 0x01, 0x01, 0x03, 0x05,
	0x03, 0x03, 0x03, 0x03, 0x05, 0x05, 0x09, 0x11,
	0x02, 0x03, 0x03, 0x02, 0x03, 0x03, 0x02, 0x06,
	0x03, 0x03, 0x03, 0x03, 0x02, 0x03, 0x06, 0x0a,
	0x04, 0x05, 0x05, 0x06, 0x05, 0x05, 0x06, 0x04,
	0x08, 0x09, 0x09, 0x0a, 0x10, 0x11, 0x20, 0x40,
	0x01, 0x01, 0x01, 0x03, 0x01, 0x01, 0x03, 0x05,
	0x01, 0x01, 0x01, 0x03, 0x03, 0x03, 0x05, 0x09,
	0x01, 0x01, 0x01, 0x03, 0x01, 0x01, 0x03, 0x05,
	0x03, 0x03, 0x03, 0x03, 0x05, 0x05, 0x09, 0x11,
	0x01, 0x01, 0x01, 0x03, 0x01, 0x01, 0x03, 0x05,
	0x01, 0x01, 0x01, 0x03, 0x03, 0x03, 0x05, 0x09,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x07,
	0x05, 0x05, 0x05, 0x07, 0x09, 0x09, 0x11, 0x21,
	0x02, 0x03, 0x03, 0x02, 0x03, 0x03, 0x02, 0x06,
	0x03, 0x03, 0x03, 0x03, 0x02, 0x03, 0x06, 0x0a,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x07,
	0x02, 0x03, 0x03, 0x02, 0x06, 0x07, 0x0a, 0x12,
	0x04, 0x05, 0x05, 0x06, 0x05, 0x05, 0x06, 0x04,
	0x05, 0x05, 0x05, 0x07, 0x06, 0x07, 0x04, 0x0c,
	0x08, 0x09, 0x09, 0x0a, 0x09, 0x09, 0x0a, 0x0c,
	0x10, 0x11, 0x11, 0x12, 0x20, 0x21, 0x40, 0x80,
};

static const u_char * const fragtbl[MAXFRAG + 1] = {
	0, fragtbl124, fragtbl124, 0, fragtbl124, 0, 0, 0, fragtbl8,
};

static daddr_t ffs_alloccg(struct inode *rip, u_int cg, daddr_t bpref,
	int size);
static daddr_t ffs_alloccgblk(struct inode *rip, struct cg *cgp, daddr_t bpref);
static daddr_t ffs_fragextend(struct inode *rip, u_int cg, daddr_t bprev,
	int osize, int nsize);
static daddr_t ffs_hashalloc(struct inode *rip, u_int cg, daddr_t bpref,
	int size, daddr_t (*allocator)(struct inode *, u_int, daddr_t, int));
static int ffs_mapsearch(struct fs *fs, struct cg *cgp, daddr_t bpref,
	int allocsiz);
static void ffs_fragacct(struct fs *fs, int fragmap, uint32_t fraglist[],
	int cnt);

/*===========================================================================*
 *			block-map bit primitives			     *
 *===========================================================================*/
/* These mirror the ffs_isblock/ffs_clrblock/ffs_setblock macros, supporting
 * fragment counts of 1, 2, 4 and 8 per block.
 */
static int ffs_isblock(struct fs *fs, u_char *cp, int32_t h)
{
  u_char mask;

  switch ((int) fs->fs_frag) {
  case 8: return(cp[h] == 0xff);
  case 4: mask = 0x0f << ((h & 0x1) << 2); return((cp[h >> 1] & mask) == mask);
  case 2: mask = 0x03 << ((h & 0x3) << 1); return((cp[h >> 2] & mask) == mask);
  case 1: mask = 0x01 << (h & 0x7); return((cp[h >> 3] & mask) == mask);
  default: panic("ffs_isblock: bad fs_frag %d", (int) fs->fs_frag);
  }
}

static void ffs_clrblock(struct fs *fs, u_char *cp, int32_t h)
{
  switch ((int) fs->fs_frag) {
  case 8: cp[h] = 0; return;
  case 4: cp[h >> 1] &= ~(0x0f << ((h & 0x1) << 2)); return;
  case 2: cp[h >> 2] &= ~(0x03 << ((h & 0x3) << 1)); return;
  case 1: cp[h >> 3] &= ~(0x01 << (h & 0x7)); return;
  default: panic("ffs_clrblock: bad fs_frag %d", (int) fs->fs_frag);
  }
}

static void ffs_setblock(struct fs *fs, u_char *cp, int32_t h)
{
  switch ((int) fs->fs_frag) {
  case 8: cp[h] = 0xff; return;
  case 4: cp[h >> 1] |= (0x0f << ((h & 0x1) << 2)); return;
  case 2: cp[h >> 2] |= (0x03 << ((h & 0x3) << 1)); return;
  case 1: cp[h >> 3] |= (0x01 << (h & 0x7)); return;
  default: panic("ffs_setblock: bad fs_frag %d", (int) fs->fs_frag);
  }
}

/*===========================================================================*
 *			cylinder-group buffering			     *
 *===========================================================================*/
/* Read cylinder group 'cg' into a freshly malloc'd, contiguous fs_bsize
 * buffer, assembled from the fs_frag fragment-sized cache blocks.  Returns
 * NULL if the cg's magic number is wrong.
 */
struct cg *ffs_read_cg(u_int cg)
{
  struct fs *fs = &superblock->s_fs;
  struct buf *bp;
  struct cg *cgp;
  char *buf;
  daddr_t base;
  int i, fsize = fs->fs_fsize;

  if ((buf = malloc(fs->fs_bsize)) == NULL)
	panic("ffs: cannot allocate cylinder-group buffer");

  base = (daddr_t) cgtod(fs, cg);
  for (i = 0; i < fs->fs_frag; i++) {
	bp = get_block(fs_dev, (block64_t) base + i, NORMAL);
	memcpy(buf + (off_t) i * fsize, b_data(bp), fsize);
	put_block(bp);
  }

  cgp = (struct cg *) buf;
  if (!cg_chkmagic(cgp)) {
	printf("ffs: bad cylinder group magic in cg %u\n", cg);
	free(buf);
	return(NULL);
  }
  return(cgp);
}

/* Write a modified cylinder group back to the cache and mark it dirty. */
void ffs_write_cg(u_int cg, struct cg *cgp)
{
  struct fs *fs = &superblock->s_fs;
  struct buf *bp;
  char *buf = (char *) cgp;
  daddr_t base;
  int i, fsize = fs->fs_fsize;

  base = (daddr_t) cgtod(fs, cg);
  for (i = 0; i < fs->fs_frag; i++) {
	bp = get_block(fs_dev, (block64_t) base + i, NO_READ);
	memcpy(b_data(bp), buf + (off_t) i * fsize, fsize);
	lmfs_markdirty(bp);
	put_block(bp);
  }
  free(buf);
}

/*===========================================================================*
 *				ffs_fragacct				     *
 *===========================================================================*/
/* Update the fragment summary counts (cg_frsum) to reflect a change of 'cnt'
 * (+1 or -1) free fragments described by the block-map byte 'fragmap'. */
static void ffs_fragacct(struct fs *fs, int fragmap, uint32_t fraglist[],
	int cnt)
{
  int inblk;
  int field, subfield;
  int siz, pos;

  inblk = (int) (fragtbl[fs->fs_frag][fragmap]) << 1;
  fragmap <<= 1;
  for (siz = 1; siz < fs->fs_frag; siz++) {
	if ((inblk & (1 << (siz + (fs->fs_frag & (NBBY - 1))))) == 0)
		continue;
	field = around[siz];
	subfield = inside[siz];
	for (pos = siz; pos <= fs->fs_frag; pos++) {
		if ((fragmap & field) == subfield) {
			fraglist[siz] += cnt;
			pos += siz;
			field <<= siz;
			subfield <<= siz;
		}
		field <<= 1;
		subfield <<= 1;
	}
  }
}

/*===========================================================================*
 *				ffs_mapsearch				     *
 *===========================================================================*/
/* Find a run of 'allocsiz' free fragments in cylinder group 'cgp', preferring
 * the position implied by 'bpref'.  Returns a cg-relative fragment number, or
 * -1 if none is found. */
static int ffs_mapsearch(struct fs *fs, struct cg *cgp, daddr_t bpref,
	int allocsiz)
{
  u_char *blksfree = cg_blksfree(cgp);
  int totbytes, start, i, loc;
  int blk, field, subfield, pos;
  daddr_t bno;

  totbytes = howmany(fs->fs_fpg, NBBY);
  if (bpref)
	start = (int) (dtogd(fs, bpref) / NBBY);
  else
	start = (int) (cgp->cg_frotor / NBBY);

  /* The per-byte table lookup mask: for fs_frag < NBBY, a fragtbl byte packs
   * the run lengths of two (or more) sub-blocks, and the "run of allocsiz
   * exists" bits start at bit fs_frag, not bit 0 (see ffs_tables.c and the
   * scanc() call in NetBSD's ffs_mapsearch).  Using bit (allocsiz - 1) alone
   * is correct only for fs_frag == 8, which is what all the newfs_ffs test
   * images happen to use; makefs images with fsize=4096/bsize=16384 have
   * fs_frag == 4 and failed every fragment allocation with ENOSPC.
   */
  loc = -1;
  for (i = 0; i < totbytes; i++) {
	int b = (start + i) % totbytes;
	if (fragtbl[fs->fs_frag][blksfree[b]] &
	    (1 << (allocsiz - 1 + (fs->fs_frag & (NBBY - 1))))) {
		loc = b;
		break;
	}
  }
  if (loc < 0)
	return(-1);

  /* Sift through the bits of the byte to find the selected fragment run. */
  for (bno = (daddr_t) loc * NBBY; bno < (daddr_t) loc * NBBY + NBBY;
       bno += fs->fs_frag) {
	blk = blkmap(fs, blksfree, bno);
	blk <<= 1;
	field = around[allocsiz];
	subfield = inside[allocsiz];
	for (pos = 0; pos <= fs->fs_frag - allocsiz; pos++) {
		if ((blk & field) == subfield) {
			cgp->cg_frotor = (u_int32_t) (bno + pos);
			return((int) (bno + pos));
		}
		field <<= 1;
		subfield <<= 1;
	}
  }

  return(-1);
}

/*===========================================================================*
 *				ffs_alloccgblk				     *
 *===========================================================================*/
/* Allocate a complete block within cylinder group 'cgp'.  Returns an absolute
 * fragment number, or 0 on failure. */
static daddr_t ffs_alloccgblk(struct inode *rip, struct cg *cgp, daddr_t bpref)
{
  struct fs *fs = &rip->i_sp->s_fs;
  u_char *blksfree = cg_blksfree(cgp);
  daddr_t bno, blkno;

  if (bpref == 0 || (u_int) dtog(fs, bpref) != cgp->cg_cgx) {
	bpref = (daddr_t) cgp->cg_rotor;
  } else {
	bpref = ffs_blknum(fs, bpref);
	bno = dtogd(fs, bpref);
	if (ffs_isblock(fs, blksfree, (int32_t) ffs_fragstoblks(fs, bno)))
		goto gotit;
  }

  bno = ffs_mapsearch(fs, cgp, bpref, (int) fs->fs_frag);
  if (bno < 0)
	return(0);
  cgp->cg_rotor = (u_int32_t) bno;

gotit:
  blkno = ffs_fragstoblks(fs, bno);
  ffs_clrblock(fs, blksfree, (int32_t) blkno);
  cgp->cg_cs.cs_nbfree--;
  fs->fs_cstotal.cs_nbfree--;
  fs->fs_csp[cgp->cg_cgx].cs_nbfree--;
  fs->fs_fmod = 1;

  return((daddr_t) cgbase(fs, cgp->cg_cgx) + bno);
}

/*===========================================================================*
 *				ffs_alloccg				     *
 *===========================================================================*/
/* Allocate a block (size == fs_bsize) or a run of fragments (size a smaller
 * multiple of fs_fsize) in cylinder group 'cg'.  Returns an absolute fragment
 * number, or 0 on failure. */
static daddr_t ffs_alloccg(struct inode *rip, u_int cg, daddr_t bpref, int size)
{
  struct fs *fs = &rip->i_sp->s_fs;
  struct cg *cgp;
  u_char *blksfree;
  daddr_t bno, blkno;
  int allocsiz, frags, i;

  cgp = ffs_read_cg(cg);
  if (cgp == NULL)
	return(0);

  if (size == fs->fs_bsize) {
	if (cgp->cg_cs.cs_nbfree == 0) {
		free(cgp);
		return(0);
	}
	bno = ffs_alloccgblk(rip, cgp, bpref);
	if (bno > 0)
		ffs_write_cg(cg, cgp);
	else
		free(cgp);
	return(bno);
  }

  /* Fragment allocation. */
  frags = numfrags(fs, size);
  blksfree = cg_blksfree(cgp);

  for (allocsiz = frags; allocsiz < fs->fs_frag; allocsiz++)
	if (cgp->cg_frsum[allocsiz] != 0)
		break;

  if (allocsiz == fs->fs_frag) {
	/* No suitable fragment run; carve one out of a whole block. */
	if (cgp->cg_cs.cs_nbfree == 0) {
		free(cgp);
		return(0);
	}
	bno = ffs_alloccgblk(rip, cgp, bpref);
	if (bno <= 0) {
		free(cgp);
		return(0);
	}
	bno = dtogd(fs, bno);		/* make cg-relative */
	for (i = frags; i < fs->fs_frag; i++)
		setbit(blksfree, bno + i);
	i = fs->fs_frag - frags;
	cgp->cg_cs.cs_nffree += i;
	fs->fs_cstotal.cs_nffree += i;
	fs->fs_csp[cg].cs_nffree += i;
	fs->fs_fmod = 1;
	cgp->cg_frsum[i]++;
	blkno = cgbase(fs, cg) + bno;
	ffs_write_cg(cg, cgp);
	return(blkno);
  }

  bno = ffs_mapsearch(fs, cgp, bpref, allocsiz);
  if (bno < 0) {
	free(cgp);
	return(0);
  }
  for (i = 0; i < frags; i++)
	clrbit(blksfree, bno + i);
  cgp->cg_cs.cs_nffree -= frags;
  fs->fs_cstotal.cs_nffree -= frags;
  fs->fs_csp[cg].cs_nffree -= frags;
  fs->fs_fmod = 1;
  cgp->cg_frsum[allocsiz]--;
  if (frags != allocsiz)
	cgp->cg_frsum[allocsiz - frags]++;
  blkno = cgbase(fs, cg) + bno;
  ffs_write_cg(cg, cgp);
  return(blkno);
}

/*===========================================================================*
 *				ffs_fragextend				     *
 *===========================================================================*/
/* Try to extend the fragment at 'bprev' from osize to nsize bytes in place.
 * Returns 'bprev' on success, or 0 if the following fragments are not free. */
static daddr_t ffs_fragextend(struct inode *rip, u_int cg, daddr_t bprev,
	int osize, int nsize)
{
  struct fs *fs = &rip->i_sp->s_fs;
  struct cg *cgp;
  u_char *blksfree;
  daddr_t bno, bbase;
  int frags, i;

  frags = numfrags(fs, nsize);

  /* An in-place extension must stay within the fragment's own block. */
  if ((int) ffs_fragnum(fs, bprev) + frags > fs->fs_frag)
	return(0);

  cgp = ffs_read_cg(cg);
  if (cgp == NULL)
	return(0);

  bno = dtogd(fs, bprev);
  blksfree = cg_blksfree(cgp);

  /* All the frags we want to add must currently be free. */
  for (i = numfrags(fs, osize); i < frags; i++)
	if (isclr(blksfree, bno + i)) {
		free(cgp);
		return(0);
	}

  /* Update the fragment-run accounting for the affected block: remove the old
   * run, clear the newly-used bits, then add back the (smaller) remaining run.
   */
  bbase = ffs_blknum(fs, bno);
  ffs_fragacct(fs, blkmap(fs, blksfree, bbase), cgp->cg_frsum, -1);
  for (i = numfrags(fs, osize); i < frags; i++)
	clrbit(blksfree, bno + i);
  ffs_fragacct(fs, blkmap(fs, blksfree, bbase), cgp->cg_frsum, 1);

  i = frags - numfrags(fs, osize);
  cgp->cg_cs.cs_nffree -= i;
  fs->fs_cstotal.cs_nffree -= i;
  fs->fs_csp[cg].cs_nffree -= i;
  fs->fs_fmod = 1;

  ffs_write_cg(cg, cgp);
  return(bprev);
}

/*===========================================================================*
 *				ffs_hashalloc				     *
 *===========================================================================*/
/* Try the preferred cylinder group; if it has no space, probe the others
 * (quadratic rehash followed by a brute-force scan). */
static daddr_t ffs_hashalloc(struct inode *rip, u_int cg, daddr_t bpref,
	int size, daddr_t (*allocator)(struct inode *, u_int, daddr_t, int))
{
  struct fs *fs = &rip->i_sp->s_fs;
  daddr_t result;
  u_int i, icg = cg;

  /* 1: the preferred cylinder group. */
  result = (*allocator)(rip, cg, bpref, size);
  if (result)
	return(result);

  /* 2: quadratic rehash. */
  for (i = 1; i < fs->fs_ncg; i *= 2) {
	cg += i;
	if (cg >= fs->fs_ncg)
		cg -= fs->fs_ncg;
	result = (*allocator)(rip, cg, 0, size);
	if (result)
		return(result);
  }

  /* 3: brute-force search. */
  cg = (icg + 2) % fs->fs_ncg;
  for (i = 2; i < fs->fs_ncg; i++) {
	result = (*allocator)(rip, cg, 0, size);
	if (result)
		return(result);
	cg++;
	if (cg == fs->fs_ncg)
		cg = 0;
  }

  return(0);
}

/*===========================================================================*
 *				ffs_alloc				     *
 *===========================================================================*/
daddr_t ffs_alloc(struct inode *rip, daddr_t lbn, daddr_t bpref, int size)
{
/* Allocate a new block or fragment of 'size' bytes for the file, returning the
 * absolute fragment number, or 0 if the file system is full. */
  struct fs *fs = &rip->i_sp->s_fs;
  daddr_t bno;
  u_int cg;

  assert(size > 0 && size <= fs->fs_bsize && ffs_fragoff(fs, size) == 0);

  if (size == fs->fs_bsize && fs->fs_cstotal.cs_nbfree == 0)
	return(0);

  if (bpref >= fs->fs_size)
	bpref = 0;
  if (bpref == 0)
	cg = (u_int) ino_to_cg(fs, rip->i_num);
  else
	cg = (u_int) dtog(fs, bpref);

  bno = ffs_hashalloc(rip, cg, bpref, size, ffs_alloccg);
  if (bno > 0) {
	rip->i_din.di_blocks += btodb(size);
	rip->i_dirt = IN_DIRTY;
	return(bno);
  }

  return(0);
}

/*===========================================================================*
 *				ffs_realloccg				     *
 *===========================================================================*/
daddr_t ffs_realloccg(struct inode *rip, daddr_t lbn, daddr_t bprev,
	daddr_t bpref, int osize, int nsize)
{
/* Grow the fragment at 'bprev' from osize to nsize bytes, either in place or
 * by allocating a new, larger fragment and copying.  Returns the new absolute
 * fragment number, or 0 on failure. */
  struct fs *fs = &rip->i_sp->s_fs;
  daddr_t bno;
  u_int cg;
  int i, request;

  if (!(osize > 0 && nsize > 0 && osize < nsize && nsize <= fs->fs_bsize))
	printf("ffs: realloccg ino=%llu lbn=%lld bprev=%lld osize=%d nsize=%d "
	    "size=%llu\n", (unsigned long long) rip->i_num, (long long) lbn,
	    (long long) bprev, osize, nsize,
	    (unsigned long long) rip->i_din.di_size);
  assert(osize > 0 && nsize > 0 && osize < nsize && nsize <= fs->fs_bsize);

  cg = (u_int) dtog(fs, bprev);

  /* 1: try to extend in place. */
  bno = ffs_fragextend(rip, cg, bprev, osize, nsize);
  if (bno != 0) {
	rip->i_din.di_blocks += btodb(nsize - osize);
	rip->i_dirt = IN_DIRTY;
	return(bno);
  }

  /* 2: allocate a new fragment, copy the data, and free the old one. */
  request = nsize;
  bno = ffs_hashalloc(rip, cg, bpref, request, ffs_alloccg);
  if (bno > 0) {
	struct buf *obp, *nbp;
	daddr_t obase, nbase;
	int ofrags = numfrags(fs, osize);

	/* Copy the existing fragments to their new location. */
	obase = bprev;
	nbase = bno;
	for (i = 0; i < ofrags; i++) {
		obp = get_block(fs_dev, (block64_t) obase + i, NORMAL);
		nbp = get_block(fs_dev, (block64_t) nbase + i, NO_READ);
		memcpy(b_data(nbp), b_data(obp), fs->fs_fsize);
		lmfs_markdirty(nbp);
		put_block(nbp);
		put_block(obp);
	}

	ffs_blkfree(rip, bprev, (long) osize);
	rip->i_din.di_blocks += btodb(nsize);
	rip->i_dirt = IN_DIRTY;
	return(bno);
  }

  return(0);
}

/*===========================================================================*
 *				ffs_blkfree				     *
 *===========================================================================*/
void ffs_blkfree(struct inode *rip, daddr_t bno, long size)
{
/* Free a block or a run of fragments of 'size' bytes at absolute fragment
 * number 'bno'. */
  struct fs *fs = &rip->i_sp->s_fs;
  struct cg *cgp;
  u_char *blksfree;
  daddr_t cgblkno;
  u_int cg;
  int i, frags, blk;

  assert(size > 0 && size <= fs->fs_bsize && ffs_fragoff(fs, size) == 0);

  cg = (u_int) dtog(fs, bno);
  cgp = ffs_read_cg(cg);
  if (cgp == NULL)
	return;

  blksfree = cg_blksfree(cgp);
  cgblkno = dtogd(fs, bno);

  if (size == fs->fs_bsize) {
	blk = (int32_t) ffs_fragstoblks(fs, cgblkno);
	/* ffs_isblock() is true when the block is already free (all frag bits
	 * set): that is a double free, so leave the counts untouched. */
	if (ffs_isblock(fs, blksfree, blk)) {
		printf("ffs: freeing free block %lld\n", (long long) bno);
		free(cgp);
		return;
	}
	ffs_setblock(fs, blksfree, blk);
	cgp->cg_cs.cs_nbfree++;
	fs->fs_cstotal.cs_nbfree++;
	fs->fs_csp[cg].cs_nbfree++;
  } else {
	frags = numfrags(fs, size);
	/* Account the fragments before they are freed... */
	ffs_fragacct(fs, blkmap(fs, blksfree, ffs_blknum(fs, cgblkno)),
		cgp->cg_frsum, -1);
	for (i = 0; i < frags; i++)
		setbit(blksfree, cgblkno + i);
	cgp->cg_cs.cs_nffree += frags;
	fs->fs_cstotal.cs_nffree += frags;
	fs->fs_csp[cg].cs_nffree += frags;
	/* ...and again after, to capture any newly-formed runs/blocks. */
	ffs_fragacct(fs, blkmap(fs, blksfree, ffs_blknum(fs, cgblkno)),
		cgp->cg_frsum, 1);
	/* If freeing these frags completes a whole block, promote it. */
	blk = (int32_t) ffs_fragstoblks(fs, ffs_blknum(fs, cgblkno));
	if (ffs_isblock(fs, blksfree, blk)) {
		cgp->cg_cs.cs_nffree -= fs->fs_frag;
		fs->fs_cstotal.cs_nffree -= fs->fs_frag;
		fs->fs_csp[cg].cs_nffree -= fs->fs_frag;
		cgp->cg_cs.cs_nbfree++;
		fs->fs_cstotal.cs_nbfree++;
		fs->fs_csp[cg].cs_nbfree++;
	}
  }

  fs->fs_fmod = 1;
  rip->i_din.di_blocks -= btodb(size);
  rip->i_dirt = IN_DIRTY;
  ffs_write_cg(cg, cgp);

  /* These fragments are no longer part of any file, so say so: VM caches
   * file pages by inode and offset, and a page that keeps the association it
   * had here would be mapped in later as the contents of whatever file --
   * or hole -- comes to own this position next.  lmfs_free_block() breaks
   * that association and drops the block from the caches. */
  for (i = 0; i < numfrags(fs, size); i++)
	lmfs_free_block(fs_dev, (block64_t) bno + i);
}

/*===========================================================================*
 *				ffs_blkpref				     *
 *===========================================================================*/
daddr_t ffs_blkpref(struct inode *rip, daddr_t lbn, int indx, int64_t *bap)
{
/* Compute a preferred fragment number for the block at logical position lbn.
 * This is a simplified version of ffs_blkpref_ufs2 without rotational or
 * maxbpg spreading; the allocator falls back to other cylinder groups when
 * the preferred one is full. */
  struct fs *fs = &rip->i_sp->s_fs;
  u_int cg;

  /* Place a block right after its predecessor when one exists. */
  if (indx > 0 && bap != NULL && bap[indx - 1] != 0)
	return((daddr_t) bap[indx - 1] + fs->fs_frag);

  /* Otherwise start at the beginning of the data area of the inode's cg. */
  cg = (u_int) ino_to_cg(fs, rip->i_num);
  return((daddr_t) cgdmin(fs, cg));
}
