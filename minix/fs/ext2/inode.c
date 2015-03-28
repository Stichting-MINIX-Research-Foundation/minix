/* This file manages the inode table.  There are procedures to allocate and
 * deallocate inodes, acquire, erase, and release them, and read and write
 * them from the disk.
 *
 * The entry points into this file are
 *   get_inode:       search inode table for a given inode; if not there,
 *                 read it
 *   put_inode:       indicate that an inode is no longer needed in memory
 *   update_times: update atime, ctime, and mtime
 *   rw_inode:       read a disk block and extract an inode, or corresp. write
 *   dup_inode:       indicate that someone else is using an inode table entry
 *   find_inode:   retrieve pointer to inode in inode cache
 *
 * Created (MFS based):
 *   February 2010 (Evgeniy Ivanov)
 */

#include "fs.h"
#include <string.h>
#include "buf.h"
#include "inode.h"
#include "super.h"
#include <minix/vfsif.h>

static void icopy(struct inode *rip, d_inode *dip, int direction, int
	norm);
static void addhash_inode(struct inode *node);
static void unhash_inode(struct inode *node);


/*===========================================================================*
 *                fs_putnode                                                 *
 *===========================================================================*/
int fs_putnode(ino_t ino_nr, unsigned int count)
{
/* Find the inode specified by the request message and decrease its counter.*/

  struct inode *rip;

  rip = find_inode(fs_dev, ino_nr);

  if (!rip) {
	printf("%s:%d put_inode: inode #%llu dev: %llx not found\n", __FILE__,
		__LINE__, ino_nr, fs_dev);
	panic("fs_putnode failed");
  }

  if (count > rip->i_count) {
	printf("%s:%d put_inode: count too high: %d > %d\n", __FILE__,
		__LINE__, count, rip->i_count);
	panic("fs_putnode failed");
  }

  /* Decrease reference counter, but keep one reference;
   * it will be consumed by put_inode().
   */
  rip->i_count -= count - 1;
  put_inode(rip);

  return(OK);
}


/*===========================================================================*
 *                init_inode_cache                                           *
 *===========================================================================*/
void init_inode_cache()
{
  struct inode *rip;
  struct inodelist *rlp;

  inode_cache_hit = 0;
  inode_cache_miss = 0;

  /* init free/unused list */
  TAILQ_INIT(&unused_inodes);

  /* init hash lists */
  for (rlp = &hash_inodes[0]; rlp < &hash_inodes[INODE_HASH_SIZE]; ++rlp)
	LIST_INIT(rlp);

  /* add free inodes to unused/free list */
  for (rip = &inode[0]; rip < &inode[NR_INODES]; ++rip) {
	rip->i_num = NO_ENTRY;
	TAILQ_INSERT_HEAD(&unused_inodes, rip, i_unused);
  }
}


/*===========================================================================*
 *                addhash_inode                                              *
 *===========================================================================*/
static void addhash_inode(struct inode *node)
{
  int hashi = node->i_num & INODE_HASH_MASK;

  /* insert into hash table */
  LIST_INSERT_HEAD(&hash_inodes[hashi], node, i_hash);
}


/*===========================================================================*
 *                unhash_inode                                               *
 *===========================================================================*/
static void unhash_inode(struct inode *node)
{
  /* remove from hash table */
  LIST_REMOVE(node, i_hash);
}


/*===========================================================================*
 *                get_inode                                                  *
 *===========================================================================*/
struct inode *get_inode(
  dev_t dev,          /* device on which inode resides */
  ino_t numb         /* inode number (ANSI: may not be unshort) */
)
{
/* Find the inode in the hash table. If it is not there, get a free inode
 * load it from the disk if it's necessary and put on the hash list
 */
  register struct inode *rip;
  int hashi;
  int i;

  hashi = (int) numb & INODE_HASH_MASK;

  /* Search inode in the hash table */
  LIST_FOREACH(rip, &hash_inodes[hashi], i_hash) {
	if (rip->i_num == numb && rip->i_dev == dev) {
		/* If unused, remove it from the unused/free list */
		if (rip->i_count == 0) {
			inode_cache_hit++;
			TAILQ_REMOVE(&unused_inodes, rip, i_unused);
		}
		++rip->i_count;
		return(rip);
	}
  }

  inode_cache_miss++;

  /* Inode is not on the hash, get a free one */
  if (TAILQ_EMPTY(&unused_inodes)) {
	err_code = ENFILE;
	return(NULL);
  }
  rip = TAILQ_FIRST(&unused_inodes);

  /* If not free unhash it */
  if (rip->i_num != NO_ENTRY)
	unhash_inode(rip);

  /* Inode is not unused any more */
  TAILQ_REMOVE(&unused_inodes, rip, i_unused);

  /* Load the inode. */
  rip->i_dev = dev;
  rip->i_num = numb;
  rip->i_count = 1;
  if (dev != NO_DEV)
	rw_inode(rip, READING);    /* get inode from disk */
  rip->i_update = 0;        /* all the times are initially up-to-date */
  rip->i_last_dpos = 0;     /* no dentries searched for yet */
  rip->i_bsearch = NO_BLOCK;
  rip->i_last_pos_bl_alloc = 0;
  rip->i_last_dentry_size = 0;
  rip->i_mountpoint= FALSE;

  rip->i_preallocation = opt.use_prealloc;
  rip->i_prealloc_count = rip->i_prealloc_index = 0;

  for (i = 0; i < EXT2_PREALLOC_BLOCKS; i++) {
	if (rip->i_prealloc_blocks[i] != NO_BLOCK) {
		/* Actually this should never happen */
		free_block(rip->i_sp, rip->i_prealloc_blocks[i]);
		rip->i_prealloc_blocks[i] = NO_BLOCK;
		ext2_debug("Warning: Unexpected preallocated block.");
	}
  }

  /* Add to hash */
  addhash_inode(rip);

  return(rip);
}


/*===========================================================================*
 *                find_inode                                                 *
 *===========================================================================*/
struct inode *find_inode(
  dev_t dev,          /* device on which inode resides */
  ino_t numb         /* inode number (ANSI: may not be unshort) */
)
{
/* Find the inode specified by the inode and device number. */
  struct inode *rip;
  int hashi;

  hashi = (int) numb & INODE_HASH_MASK;

  /* Search inode in the hash table */
  LIST_FOREACH(rip, &hash_inodes[hashi], i_hash) {
	if (rip->i_count > 0 && rip->i_num == numb && rip->i_dev == dev) {
		return(rip);
	}
  }

  return(NULL);
}


/*===========================================================================*
 *                put_inode                                                  *
 *===========================================================================*/
void put_inode(
  register struct inode *rip     /* pointer to inode to be released */
)
{
/* The caller is no longer using this inode. If no one else is using it either
 * write it back to the disk immediately. If it has no links, truncate it and
 * return it to the pool of available inodes.
 */

  if (rip == NULL)
	return;    /* checking here is easier than in caller */

  if (rip->i_count < 1)
	panic("put_inode: i_count already below 1: %d", rip->i_count);

  if (--rip->i_count == 0) {    /* i_count == 0 means no one is using it now */
	if (rip->i_links_count == NO_LINK) {
		/* i_nlinks == NO_LINK means free the inode. */
		/* return all the disk blocks */

		/* Ignore errors by truncate_inode in case inode is a block
		 * special or character special file.
		 */
		(void) truncate_inode(rip, (off_t) 0);
		/* free inode clears I_TYPE field, since it's used there */
		rip->i_dirt = IN_DIRTY;
		free_inode(rip);
	}

	rip->i_mountpoint = FALSE;
	if (rip->i_dirt == IN_DIRTY) rw_inode(rip, WRITING);

	discard_preallocated_blocks(rip); /* Return blocks to the filesystem */

	if (rip->i_links_count == NO_LINK) {
		/* free, put at the front of the LRU list */
		unhash_inode(rip);
		rip->i_num = NO_ENTRY;
		TAILQ_INSERT_HEAD(&unused_inodes, rip, i_unused);
	} else {
		/* unused, put at the back of the LRU (cache it) */
		TAILQ_INSERT_TAIL(&unused_inodes, rip, i_unused);
	}
  }
}


/*===========================================================================*
 *                update_times                                               *
 *===========================================================================*/
void update_times(
  register struct inode *rip     /* pointer to inode to be read/written */
)
{
/* Various system calls are required by the standard to update atime, ctime,
 * or mtime.  Since updating a time requires sending a message to the clock
 * task--an expensive business--the times are marked for update by setting
 * bits in i_update. When a stat, fstat, or sync is done, or an inode is
 * released, update_times() may be called to actually fill in the times.
 */

  time_t cur_time;
  struct super_block *sp;

  sp = rip->i_sp;         /* get pointer to super block. */
  if (sp->s_rd_only)
	return;             /* no updates for read-only file systems */

  cur_time = clock_time(NULL);
  if (rip->i_update & ATIME)
	rip->i_atime = cur_time;
  if (rip->i_update & CTIME)
	rip->i_ctime = cur_time;
  if (rip->i_update & MTIME)
	rip->i_mtime = cur_time;
  rip->i_update = 0;          /* they are all up-to-date now */
}

/*===========================================================================*
 *                rw_inode                                                   *
 *===========================================================================*/
void rw_inode(
  register struct inode *rip,         /* pointer to inode to be read/written */
  int rw_flag                         /* READING or WRITING */
)
{
/* An entry in the inode table is to be copied to or from the disk. */

  register struct buf *bp;
  register struct super_block *sp;
  register struct group_desc *gd;
  register d_inode *dip;
  u32_t block_group_number;
  block_t b, offset;

  /* Get the block where the inode resides. */
  sp = get_super(rip->i_dev);     /* get pointer to super block */
  rip->i_sp = sp;        /* inode must contain super block pointer */

  block_group_number = (rip->i_num - 1) / sp->s_inodes_per_group;

  gd = get_group_desc(block_group_number);

  if (gd == NULL)
	panic("can't get group_desc to read/write inode");

  offset = ((rip->i_num - 1) % sp->s_inodes_per_group) * EXT2_INODE_SIZE(sp);
  /* offset requires shifting, since each block contains several inodes,
   * e.g. inode 2 is stored in bklock 0.
   */
  b = (block_t) gd->inode_table + (offset >> sp->s_blocksize_bits);
  bp = get_block(rip->i_dev, b, NORMAL);

  offset &= (sp->s_block_size - 1);
  dip = (d_inode*) (b_data(bp) + offset);

  /* Do the read or write. */
  if (rw_flag == WRITING) {
	if (rip->i_update)
		update_times(rip);    /* times need updating */
	if (sp->s_rd_only == FALSE)
		lmfs_markdirty(bp);
  }

  icopy(rip, dip, rw_flag, TRUE);

  put_block(bp);
  rip->i_dirt = IN_CLEAN;
}


/*===========================================================================*
 *				icopy					     *
 *===========================================================================*/
static void icopy(
  register struct inode *rip,	/* pointer to the in-core inode struct */
  register d_inode *dip,	/* pointer to the on-disk struct */
  int direction,		/* READING (from disk) or WRITING (to disk) */
  int norm			/* TRUE = do not swap bytes; FALSE = swap */
)
{
  int i;

  if (direction == READING) {
	/* Copy inode to the in-core table, swapping bytes if need be. */
	rip->i_mode    = conv2(norm,dip->i_mode);
	rip->i_uid     = conv2(norm,dip->i_uid);
	rip->i_size    = conv4(norm,dip->i_size);
	rip->i_atime   = conv4(norm,dip->i_atime);
	rip->i_ctime   = conv4(norm,dip->i_ctime);
	rip->i_mtime   = conv4(norm,dip->i_mtime);
	rip->i_dtime   = conv4(norm,dip->i_dtime);
	rip->i_gid     = conv2(norm,dip->i_gid);
	rip->i_links_count  = conv2(norm,dip->i_links_count);
	rip->i_blocks	= conv4(norm,dip->i_blocks);
	rip->i_flags	= conv4(norm,dip->i_flags);
	/* Minix doesn't touch osd1 and osd2 either, so just copy. */
	memcpy(&rip->osd1, &dip->osd1, sizeof(rip->osd1));
	for (i = 0; i < EXT2_N_BLOCKS; i++)
		rip->i_block[i] = conv4(norm, dip->i_block[i]);
	rip->i_generation = conv4(norm,dip->i_generation);
	rip->i_file_acl	= conv4(norm,dip->i_file_acl);
	rip->i_dir_acl  = conv4(norm,dip->i_dir_acl);
	rip->i_faddr	= conv4(norm,dip->i_faddr);
	memcpy(&rip->osd2, &dip->osd2, sizeof(rip->osd2));
  } else {
	/* Copying inode to disk from the in-core table. */
	dip->i_mode    = conv2(norm,rip->i_mode);
	dip->i_uid     = conv2(norm,rip->i_uid);
	dip->i_size    = conv4(norm,rip->i_size);
	dip->i_atime   = conv4(norm,rip->i_atime);
	dip->i_ctime   = conv4(norm,rip->i_ctime);
	dip->i_mtime   = conv4(norm,rip->i_mtime);
	dip->i_dtime   = conv4(norm,rip->i_dtime);
	dip->i_gid     = conv2(norm,rip->i_gid);
	dip->i_links_count  = conv2(norm,rip->i_links_count);
	dip->i_blocks	= conv4(norm,rip->i_blocks);
	dip->i_flags	= conv4(norm,rip->i_flags);
	/* Minix doesn't touch osd1 and osd2 either, so just copy. */
	memcpy(&dip->osd1, &rip->osd1, sizeof(dip->osd1));
	for (i = 0; i < EXT2_N_BLOCKS; i++)
		dip->i_block[i] = conv4(norm, rip->i_block[i]);
	dip->i_generation  = conv4(norm,rip->i_generation);
	dip->i_file_acl = conv4(norm,rip->i_file_acl);
	dip->i_dir_acl	= conv4(norm,rip->i_dir_acl);
	dip->i_faddr	= conv4(norm,rip->i_faddr);
	memcpy(&dip->osd2, &rip->osd2, sizeof(dip->osd2));
  }
}


/*===========================================================================*
 *                dup_inode                                                  *
 *===========================================================================*/
void dup_inode(
  struct inode *ip         /* The inode to be duplicated. */
)
{
/* This routine is a simplified form of get_inode() for the case where
 * the inode pointer is already known.
 */
  ip->i_count++;
}
