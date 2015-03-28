/* This files manages inodes allocation and deallocation.
 *
 * The entry points into this file are:
 *   alloc_inode:  allocate a new, unused inode.
 *   free_inode:   mark an inode as available for a new file.
 *
 * Created (alloc_inode/free_inode/wipe_inode are from MFS):
 *   June 2010 (Evgeniy Ivanov)
 */

#include "fs.h"
#include <string.h>
#include <stdlib.h>
#include <minix/com.h>
#include <minix/u64.h>
#include "buf.h"
#include "inode.h"
#include "super.h"
#include "const.h"


static bit_t alloc_inode_bit(struct super_block *sp, struct inode
	*parent, int is_dir);
static void free_inode_bit(struct super_block *sp, bit_t bit_returned,
	int is_dir);
static void wipe_inode(struct inode *rip);


/*===========================================================================*
 *                alloc_inode                                                *
 *===========================================================================*/
struct inode *alloc_inode(struct inode *parent, mode_t bits, uid_t uid,
	gid_t gid)
{
/* Allocate a free inode on parent's dev, and return a pointer to it. */

  register struct inode *rip;
  register struct super_block *sp;
  int inumb;
  bit_t b;
  static int print_oos_msg = 1;

  sp = get_super(parent->i_dev);    /* get pointer to super_block */
  if (sp->s_rd_only) {    /* can't allocate an inode on a read only device. */
	err_code = EROFS;
	return(NULL);
  }

  /* Acquire an inode from the bit map. */
  b = alloc_inode_bit(sp, parent, (bits & I_TYPE) == I_DIRECTORY);
  if (b == NO_BIT) {
	err_code = ENOSPC;
	if (print_oos_msg)
		ext2_debug("Out of i-nodes on device %d/%d\n",
			   major(sp->s_dev), minor(sp->s_dev));
	print_oos_msg = 0;	/* Don't repeat message */
	return(NULL);
  }
  print_oos_msg = 1;

  inumb = (int) b;        /* be careful not to pass unshort as param */

  /* Try to acquire a slot in the inode table. */
  if ((rip = get_inode(NO_DEV, inumb)) == NULL) {
	/* No inode table slots available.  Free the inode just allocated. */
	free_inode_bit(sp, b, (bits & I_TYPE) == I_DIRECTORY);
  } else {
	/* An inode slot is available. Put the inode just allocated into it. */
	rip->i_mode = bits;         /* set up RWX bits */
	rip->i_links_count = NO_LINK; /* initial no links */
	rip->i_uid = uid;           /* file's uid is owner's */
	rip->i_gid = gid;           /* ditto group id */
	rip->i_dev = parent->i_dev; /* mark which device it is on */
	rip->i_sp = sp;             /* pointer to super block */

	/* Fields not cleared already are cleared in wipe_inode(). They have
	 * been put there because truncate() needs to clear the same fields if
	 * the file happens to be open while being truncated. It saves space
	 * not to repeat the code twice.
	 */
	wipe_inode(rip);
  }

  return(rip);
}


/*===========================================================================*
 *                free_inode                                                 *
 *===========================================================================*/
void free_inode(
  register struct inode *rip  /* inode to free */
)
{
/* Return an inode to the pool of unallocated inodes. */
  register struct super_block *sp;
  dev_t dev = rip->i_dev;
  bit_t b = rip->i_num;
  u16_t mode = rip->i_mode;

  /* Locate the appropriate super_block. */
  sp = get_super(dev);

  if (b <= NO_ENTRY || b > sp->s_inodes_count)
	return;
  free_inode_bit(sp, b, (mode & I_TYPE) == I_DIRECTORY);

  rip->i_mode = I_NOT_ALLOC;     /* clear I_TYPE field */
}


static int find_group_dir(struct super_block *sp);
static int find_group_hashalloc(struct super_block *sp, struct inode
	*parent);
static int find_group_any(struct super_block *sp);
static int find_group_orlov(struct super_block *sp, struct inode
	*parent);


/*===========================================================================*
 *                              alloc_inode_bit                              *
 *===========================================================================*/
static bit_t alloc_inode_bit(sp, parent, is_dir)
struct super_block *sp;         /* the filesystem to allocate from */
struct inode *parent;		/* parent of newly allocated inode */
int is_dir;			/* inode will be a directory if it is TRUE */
{
  int group;
  ino_t inumber = NO_BIT;
  bit_t bit;
  struct buf *bp;
  struct group_desc *gd;

  if (sp->s_rd_only)
	panic("can't alloc inode on read-only filesys.");

  if (opt.mfsalloc) {
	group = find_group_any(sp);
  } else {
	if (is_dir) {
		if (opt.use_orlov) {
			group = find_group_orlov(sp, parent);
		} else {
			group = find_group_dir(sp);
		}
	} else {
		group = find_group_hashalloc(sp, parent);
	}
  }
  /* Check if we have a group where to allocate an inode */
  if (group == -1)
	return(NO_BIT);	/* no bit could be allocated */

  gd = get_group_desc(group);
  if (gd == NULL)
	  panic("can't get group_desc to alloc block");

  /* find_group_* should always return either a group with
   * a free inode slot or -1, which we checked earlier.
   */
  ASSERT(gd->free_inodes_count);

  bp = get_block(sp->s_dev, gd->inode_bitmap, NORMAL);
  bit = setbit(b_bitmap(bp), sp->s_inodes_per_group, 0);
  ASSERT(bit != -1); /* group definitly contains free inode */

  inumber = group * sp->s_inodes_per_group + bit + 1;

  /* Extra checks before real allocation.
   * Only major bug can cause problems. Since setbit changed
   * bp->b_bitmap there is no way to recover from this bug.
   * Should never happen.
   */
  if (inumber > sp->s_inodes_count) {
	panic("ext2: allocator returned inum greater, than\
		    total number of inodes.\n");
  }

  if (inumber < EXT2_FIRST_INO(sp)) {
	panic("ext2: allocator tryed to use reserved inode.\n");
  }

  lmfs_markdirty(bp);
  put_block(bp);

  gd->free_inodes_count--;
  sp->s_free_inodes_count--;
  if (is_dir) {
	gd->used_dirs_count++;
	sp->s_dirs_counter++;
  }

  group_descriptors_dirty = 1;

  /* Almost the same as previous 'group' ASSERT */
  ASSERT(inumber != NO_BIT);
  return inumber;
}


/*===========================================================================*
 *                          free_inode_bit                                   *
 *===========================================================================*/
static void free_inode_bit(struct super_block *sp, bit_t bit_returned,
                           int is_dir)
{
 /* Return an inode by turning off its bitmap bit. */
  int group;		/* group number of bit_returned */
  int bit;		/* bit_returned number within its group */
  struct buf *bp;
  struct group_desc *gd;

  if (sp->s_rd_only)
	panic("can't free bit on read-only filesys.");

  /* At first search group, to which bit_returned belongs to
   * and figure out in what word bit is stored.
   */
  if (bit_returned > sp->s_inodes_count ||
      bit_returned < EXT2_FIRST_INO(sp))
	panic("trying to free inode %d beyond inodes scope.", bit_returned);

  group = (bit_returned - 1) / sp->s_inodes_per_group;
  bit = (bit_returned - 1) % sp->s_inodes_per_group; /* index in bitmap */

  gd = get_group_desc(group);
  if (gd == NULL)
	panic("can't get group_desc to alloc block");

  bp = get_block(sp->s_dev, gd->inode_bitmap, NORMAL);

  if (unsetbit(b_bitmap(bp), bit))
	panic("Tried to free unused inode %d", bit_returned);

  lmfs_markdirty(bp);
  put_block(bp);

  gd->free_inodes_count++;
  sp->s_free_inodes_count++;

  if (is_dir) {
	gd->used_dirs_count--;
	sp->s_dirs_counter--;
  }

  group_descriptors_dirty = 1;

  if (group < sp->s_igsearch)
	sp->s_igsearch = group;
}


/* it's implemented very close to the linux' find_group_dir() */
static int find_group_dir(struct super_block *sp)
{
  int avefreei = sp->s_free_inodes_count / sp->s_groups_count;
  struct group_desc *gd, *best_gd = NULL;
  int group, best_group = -1;

  for (group = 0; group < sp->s_groups_count; ++group) {
	gd = get_group_desc(group);
	if (gd == NULL)
		panic("can't get group_desc to alloc inode");
	if (gd->free_inodes_count == 0)
		continue;
	if (gd->free_inodes_count < avefreei)
		continue;
	if (!best_gd ||
	     gd->free_blocks_count > best_gd->free_blocks_count) {
		best_gd = gd;
		best_group = group;
	}
  }

  return best_group; /* group or -1 */
}


/* Analog of ffs_hashalloc() from *BSD.
 * 1) Check parent's for free inodes and blocks.
 * 2) Quadradically rehash on the group number.
 * 3) Make a linear search for free inode.
 */
static int find_group_hashalloc(struct super_block *sp, struct inode *parent)
{
  int ngroups = sp->s_groups_count;
  struct group_desc *gd;
  int group, i;
  int parent_group = (parent->i_num - 1) / sp->s_inodes_per_group;

  /* Try to place new inode in its parent group */
  gd = get_group_desc(parent_group);
  if (gd == NULL)
	panic("can't get group_desc to alloc inode");
  if (gd->free_inodes_count && gd->free_blocks_count)
	return parent_group;

  /* We can't allocate inode in the parent's group.
   * Now we will try to place it in another blockgroup.
   * The main idea is still to keep files from the same
   * directory together and use different blockgroups for
   * files from another directory, which lives in the same
   * blockgroup as our parent.
   * Thus we will spread things on the disk.
   */
  group = (parent_group + parent->i_num) % ngroups;

  /* Make quadratic probing to find a group with free inodes and blocks. */
  for (i = 1; i < ngroups; i <<= 1) {
	group += i;
	if (group >= ngroups)
		group -= ngroups;
	gd = get_group_desc(group);
	if (gd == NULL)
		panic("can't get group_desc to alloc inode");
	if (gd->free_inodes_count && gd->free_blocks_count)
		return group;
  }

  /* Still no group for new inode, try linear search.
   * Also check parent again (but for free inodes only).
   */
  group = parent_group;
  for (i = 0; i < ngroups; i++, group++) {
	if (group >= ngroups)
		group = 0;
	gd = get_group_desc(group);
	if (gd == NULL)
		panic("can't get group_desc to alloc inode");
	if (gd->free_inodes_count)
		return group;
  }

  return -1;
}


/* Find first group which has free inode slot.
 * This is similar to what MFS does.
 */
static int find_group_any(struct super_block *sp)
{
  int ngroups = sp->s_groups_count;
  struct group_desc *gd;
  int group = sp->s_igsearch;

  for (; group < ngroups; group++) {
	gd = get_group_desc(group);
	if (gd == NULL)
		panic("can't get group_desc to alloc inode");
	if (gd->free_inodes_count) {
		sp->s_igsearch = group;
		return group;
	}
  }

  return -1;
}


/* We try to spread first-level directories (i.e. directories in the root
 * or in the directory marked as TOPDIR).
 * If there are blockgroups with counts for blocks and inodes less than average
 * we return a group with lowest directory count. Otherwise we either
 * return a group with good free inodes and blocks counts or just a group
 * with free inode.
 *
 * For other directories we try to find a 'good' group, we consider a group as
 * a 'good' if it has enough blocks and inodes (greater than min_blocks and
 * min_inodes).
 *
 */
static int find_group_orlov(struct super_block *sp, struct inode *parent)
{
  int avefreei = sp->s_free_inodes_count / sp->s_groups_count;
  int avefreeb = sp->s_free_blocks_count / sp->s_groups_count;

  int group = -1;
  int fallback_group = -1; /* Group with at least 1 free inode */
  struct group_desc *gd;
  int i;

  if (parent->i_num == ROOT_INODE ||
      parent->i_flags & EXT2_TOPDIR_FL) {
	int best_group = -1;
	int best_avefree_group = -1; /* Best value of avefreei/avefreeb */
	int best_ndir = sp->s_inodes_per_group;

	group = (unsigned int)random();
	for (i = 0; i < sp->s_groups_count; i++, group++) {
		if (group >= sp->s_groups_count)
			group = 0;
		gd = get_group_desc(group);
		if (gd == NULL)
			panic("can't get group_desc to alloc inode");
		if (gd->free_inodes_count == 0)
			continue;

		fallback_group = group;

		if (gd->free_inodes_count < avefreei ||
		    gd->free_blocks_count < avefreeb)
			continue;

		best_avefree_group  = group;

		if (gd->used_dirs_count >= best_ndir)
			continue;
		best_ndir = gd->used_dirs_count;
		best_group = group;
	}
	if (best_group >= 0)
		return best_group;
	if (best_avefree_group >= 0)
		return best_avefree_group;
	return fallback_group;
  } else {
	int parent_group = (parent->i_num - 1) / sp->s_inodes_per_group;
	/* 2 is kind of random thing for now,
	 * but performance results are still good.
	 */
	int min_blocks = avefreeb / 2;
	int min_inodes = avefreei / 2;

	group = parent_group;
	for (i = 0; i < sp->s_groups_count; i++, group++) {
		if (group >= sp->s_groups_count)
			group = 0;
		gd = get_group_desc(group);
		if (gd == NULL)
			panic("can't get group_desc to alloc inode");
		if (gd->free_inodes_count == 0)
			continue;

		fallback_group = group;

		if (gd->free_inodes_count >= min_inodes &&
		    gd->free_blocks_count >= min_blocks)
			return group;
	}
	return fallback_group;
  }

  return -1;
}


/*===========================================================================*
 *                wipe_inode                                                 *
 *===========================================================================*/
static void wipe_inode(
  register struct inode *rip     /* the inode to be erased */
)
{
/* Erase some fields in the inode. This function is called from alloc_inode()
 * when a new inode is to be allocated, and from truncate(), when an existing
 * inode is to be truncated.
 */

  register int i;

  rip->i_size = 0;
  rip->i_update = ATIME | CTIME | MTIME;    /* update all times later */
  rip->i_blocks = 0;
  rip->i_flags = 0;
  rip->i_generation = 0;
  rip->i_file_acl = 0;
  rip->i_dir_acl = 0;
  rip->i_faddr = 0;

  for (i = 0; i < EXT2_N_BLOCKS; i++)
	rip->i_block[i] = NO_BLOCK;
  rip->i_block[0] = NO_BLOCK;

  rip->i_dirt = IN_DIRTY;
}
