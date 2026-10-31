/* Inode allocation and freeing for UFS2.
 *
 * Ported from the inode paths of NetBSD's ffs_alloc.c (ffs_valloc,
 * ffs_nodealloccg, ffs_freefile), adapted to MINIX and the contiguous
 * cylinder-group buffer model in balloc.c.
 *
 * UFS2 lazily initializes inode blocks: cg_initediblk counts how many inodes
 * in the group have had their on-disk block zeroed.  When we hand out an inode
 * beyond that point we must first zero the covering inode block(s) on disk, or
 * a later read of the inode would return garbage.
 */

#include "fs.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "buf.h"
#include "inode.h"
#include "super.h"

/* cylinder-group buffer helpers (balloc.c) */
struct cg *ffs_read_cg(u_int cg);
void ffs_write_cg(u_int cg, struct cg *cgp);

/*===========================================================================*
 *				ffs_dirpref				     *
 *===========================================================================*/
/* Pick a cylinder group for a new directory.  A simplified policy: choose the
 * group with the most free inodes (the original FFS uses the Orlov allocator). */
static u_int ffs_dirpref(struct fs *fs)
{
  u_int cg, best = 0;
  int64_t maxfree = -1;

  for (cg = 0; cg < fs->fs_ncg; cg++) {
	if (fs->fs_csp[cg].cs_nifree > maxfree) {
		maxfree = fs->fs_csp[cg].cs_nifree;
		best = cg;
	}
  }
  return(best);
}

/*===========================================================================*
 *				init_inode_block			     *
 *===========================================================================*/
/* Zero the on-disk inode block that contains inode index 'ediblk' within
 * cylinder group 'cg', giving every inode a nonzero generation number. */
static void init_inode_block(struct fs *fs, u_int cg, int32_t ediblk)
{
  struct buf *bp;
  struct ufs2_dinode *dp;
  daddr_t fsba;
  ino_t base;
  int i, j, per_frag;

  base = (ino_t) cg * fs->fs_ipg + ediblk;
  fsba = (daddr_t) ino_to_fsba(fs, base);
  per_frag = fs->fs_fsize / (int) DINODE2_SIZE;

  for (i = 0; i < fs->fs_frag; i++) {
	bp = get_block(fs_dev, (block64_t) fsba + i, NO_READ);
	memset(b_data(bp), 0, fs->fs_fsize);
	dp = (struct ufs2_dinode *) b_data(bp);
	for (j = 0; j < per_frag; j++)
		dp[j].di_gen = 1;
	lmfs_markdirty(bp);
	put_block(bp);
  }
}

/*===========================================================================*
 *				ffs_nodealloccg				     *
 *===========================================================================*/
/* Allocate an inode in cylinder group 'cg', preferring index 'ipref'.  Returns
 * the absolute inode number, or 0 if the group is full. */
static ino_t ffs_nodealloccg(struct fs *fs, u_int cg, ino_t ipref, mode_t mode)
{
  struct cg *cgp;
  u_int8_t *inosused;
  int32_t initediblk;
  ino_t ino;
  u_int i, len;

  cgp = ffs_read_cg(cg);
  if (cgp == NULL)
	return(0);
  if (cgp->cg_cs.cs_nifree == 0) {
	free(cgp);
	return(0);
  }

  inosused = cg_inosused(cgp);

  /* Honor the preferred index if it is free and already initialized. */
  if (ipref) {
	ipref %= fs->fs_ipg;
	if (isclr(inosused, ipref))
		goto gotit;
  }

  /* Otherwise scan the in-use bitmap for the first free inode. */
  len = (u_int) fs->fs_ipg;
  ipref = (ino_t) -1;
  for (i = 0; i < len; i++) {
	if (isclr(inosused, i)) {
		ipref = i;
		break;
	}
  }
  if (ipref == (ino_t) -1) {
	free(cgp);
	return(0);		/* should not happen: cs_nifree said otherwise */
  }

gotit:
  /* Initialize the inode block(s) up to and including this inode if needed.
   * This is a UFS2-only optimization: UFS1 initializes every inode block at
   * newfs time and has no cg_initediblk counter (the field would be garbage),
   * so skip it there. */
  if (fs->fs_magic != FS_UFS1_MAGIC) {
	initediblk = cgp->cg_initediblk;
	while (ipref >= (ino_t) initediblk &&
	    initediblk < (int32_t) cgp->cg_niblk) {
		init_inode_block(fs, cg, initediblk);
		initediblk += FFS_INOPB(fs);
	}
	cgp->cg_initediblk = initediblk;
  }

  setbit(inosused, ipref);
  if (ipref >= cgp->cg_irotor)
	cgp->cg_irotor = (u_int32_t) ipref;

  cgp->cg_cs.cs_nifree--;
  fs->fs_cstotal.cs_nifree--;
  fs->fs_csp[cg].cs_nifree--;
  if ((mode & I_TYPE) == I_DIRECTORY) {
	cgp->cg_cs.cs_ndir++;
	fs->fs_cstotal.cs_ndir++;
	fs->fs_csp[cg].cs_ndir++;
  }
  fs->fs_fmod = 1;

  ino = (ino_t) cg * fs->fs_ipg + ipref;
  ffs_write_cg(cg, cgp);
  return(ino);
}

/*===========================================================================*
 *				alloc_inode				     *
 *===========================================================================*/
struct inode *alloc_inode(struct inode *parent, mode_t bits, uid_t uid,
	gid_t gid)
{
/* Allocate a free inode on the same device as 'parent', initialize the new
 * in-core inode, and return it (with one reference held). */
  struct super_block *sp;
  struct fs *fs;
  struct inode *rip;
  ino_t ino;
  u_int cg;
  u_int32_t gen;

  sp = parent->i_sp;
  fs = &sp->s_fs;

  if (sp->s_rd_only) {
	err_code = EROFS;
	return(NULL);
  }

  if ((bits & I_TYPE) == I_DIRECTORY)
	cg = ffs_dirpref(fs);
  else
	cg = (u_int) ino_to_cg(fs, parent->i_num);

  /* Try the preferred group, then probe the rest of the file system. */
  ino = ffs_nodealloccg(fs, cg, 0, bits);
  if (ino == 0) {
	u_int i;
	for (i = 1; i < fs->fs_ncg && ino == 0; i++)
		ino = ffs_nodealloccg(fs, (cg + i) % fs->fs_ncg, 0, bits);
  }
  if (ino == 0) {
	err_code = ENOSPC;
	return(NULL);
  }

  if ((rip = get_inode(fs_dev, ino)) == NULL)
	return(NULL);

  /* Initialize the new inode, preserving a fresh generation number. */
  gen = rip->i_din.di_gen;
  memset(&rip->i_din, 0, sizeof(rip->i_din));
  rip->i_din.di_gen = gen ? gen : 1;
  rip->i_din.di_mode = (u_int16_t) bits;
  rip->i_din.di_uid = uid;
  rip->i_din.di_gid = gid;
  rip->i_din.di_nlink = 0;	/* caller bumps the link count */
  rip->i_update = ATIME | CTIME | MTIME;
  rip->i_dirt = IN_DIRTY;

  return(rip);
}

/*===========================================================================*
 *				free_inode				     *
 *===========================================================================*/
void free_inode(struct inode *rip)
{
/* Return inode 'rip' to its cylinder group and clear its on-disk image. */
  struct fs *fs = &rip->i_sp->s_fs;
  struct cg *cgp;
  u_int8_t *inosused;
  mode_t mode;
  ino_t ino, rel;
  u_int cg;
  u_int32_t gen;

  if (rip->i_sp->s_rd_only)
	return;

  ino = rip->i_num;
  mode = rip->i_din.di_mode;
  cg = (u_int) ino_to_cg(fs, ino);
  rel = ino % fs->fs_ipg;

  cgp = ffs_read_cg(cg);
  if (cgp == NULL)
	return;

  inosused = cg_inosused(cgp);
  if (isclr(inosused, rel)) {
	printf("ffs: freeing free inode %llu\n", (unsigned long long) ino);
	free(cgp);
	return;
  }
  clrbit(inosused, rel);
  if (rel < cgp->cg_irotor)
	cgp->cg_irotor = (u_int32_t) rel;

  cgp->cg_cs.cs_nifree++;
  fs->fs_cstotal.cs_nifree++;
  fs->fs_csp[cg].cs_nifree++;
  if ((mode & I_TYPE) == I_DIRECTORY) {
	cgp->cg_cs.cs_ndir--;
	fs->fs_cstotal.cs_ndir--;
	fs->fs_csp[cg].cs_ndir--;
  }
  fs->fs_fmod = 1;
  ffs_write_cg(cg, cgp);

  /* Clear the on-disk inode, keeping a bumped generation number. */
  gen = rip->i_din.di_gen;
  memset(&rip->i_din, 0, sizeof(rip->i_din));
  rip->i_din.di_gen = gen + 1;
  rip->i_dirt = IN_DIRTY;
}
