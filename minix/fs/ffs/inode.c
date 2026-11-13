/* This file manages the inode table.  There are procedures to acquire, erase,
 * and release inodes, and to read and write them from the disk.
 *
 * The entry points into this file are:
 *   get_inode:        search the inode table for an inode; read it if absent
 *   put_inode:        indicate that an inode is no longer needed in memory
 *   find_inode:       return a pointer to an in-core inode, or NULL
 *   dup_inode:        indicate that someone else is using an inode slot
 *   update_times:     update atime, ctime, and mtime
 *   rw_inode:         read or write the on-disk inode
 *   fs_putnode:       VFS request to drop a reference to an inode
 *   fs_seek:          VFS notification that a seek happened
 */

#include "fs.h"
#include <string.h>
#include "buf.h"
#include "inode.h"
#include "super.h"
#include <minix/vfsif.h>

static void addhash_inode(struct inode *node);
static void unhash_inode(struct inode *node);

/*===========================================================================*
 *                fs_putnode                                                 *
 *===========================================================================*/
int fs_putnode(ino_t ino_nr, unsigned int count)
{
/* Find the inode specified by the request and decrease its reference count. */
  struct inode *rip;

  rip = find_inode(fs_dev, ino_nr);

  if (!rip) {
	printf("%s:%d put_inode: inode #%llu dev: %llx not found\n", __FILE__,
		__LINE__, (unsigned long long)ino_nr, (unsigned long long)fs_dev);
	panic("fs_putnode failed");
  }

  if (count > (unsigned int) rip->i_count) {
	printf("%s:%d put_inode: count too high: %u > %d\n", __FILE__,
		__LINE__, count, rip->i_count);
	panic("fs_putnode failed");
  }

  /* Decrease reference counter, but keep one reference; it will be consumed
   * by put_inode() below.
   */
  rip->i_count -= count - 1;
  put_inode(rip);

  return(OK);
}

/*===========================================================================*
 *                init_inode_cache                                           *
 *===========================================================================*/
void init_inode_cache(void)
{
  struct inode *rip;
  struct inodelist *rlp;

  /* init free/unused list */
  TAILQ_INIT(&unused_inodes);

  /* init hash lists */
  for (rlp = &hash_inodes[0]; rlp < &hash_inodes[INODE_HASH_SIZE]; ++rlp)
	LIST_INIT(rlp);

  /* add free inodes to unused/free list */
  for (rip = &inode[0]; rip < &inode[NR_INODES]; ++rip) {
	rip->i_num = NO_ENTRY;
	rip->i_count = 0;
	TAILQ_INSERT_HEAD(&unused_inodes, rip, i_unused);
  }
}

/*===========================================================================*
 *                addhash_inode                                              *
 *===========================================================================*/
static void addhash_inode(struct inode *node)
{
  int hashi = (int) (node->i_num & INODE_HASH_MASK);

  LIST_INSERT_HEAD(&hash_inodes[hashi], node, i_hash);
}

/*===========================================================================*
 *                unhash_inode                                               *
 *===========================================================================*/
static void unhash_inode(struct inode *node)
{
  LIST_REMOVE(node, i_hash);
}

/*===========================================================================*
 *                get_inode                                                  *
 *===========================================================================*/
struct inode *get_inode(dev_t dev, ino_t numb)
{
/* Find the inode in the hash table.  If it is not there, get a free inode,
 * load it from the disk, and put it on the hash list.
 */
  struct inode *rip;
  int hashi;

  hashi = (int) (numb & INODE_HASH_MASK);

  /* Search inode in the hash table. */
  LIST_FOREACH(rip, &hash_inodes[hashi], i_hash) {
	if (rip->i_num == numb && rip->i_dev == dev) {
		/* If unused, remove it from the unused/free list. */
		if (rip->i_count == 0)
			TAILQ_REMOVE(&unused_inodes, rip, i_unused);
		++rip->i_count;
		return(rip);
	}
  }

  /* Inode is not in the hash; get a free one. */
  if (TAILQ_EMPTY(&unused_inodes)) {
	err_code = ENFILE;
	return(NULL);
  }
  rip = TAILQ_FIRST(&unused_inodes);

  /* If not free, unhash it. */
  if (rip->i_num != NO_ENTRY)
	unhash_inode(rip);

  TAILQ_REMOVE(&unused_inodes, rip, i_unused);

  /* Load the inode. */
  rip->i_dev = dev;
  rip->i_num = numb;
  rip->i_count = 1;
  if (dev != NO_DEV)
	rw_inode(rip, READING);		/* get inode from disk */
  rip->i_update = 0;			/* all times initially up-to-date */
  rip->i_last_dpos = 0;			/* no dentries searched for yet */
  rip->i_last_dentry_size = 0;
  rip->i_mountpoint = FALSE;
  rip->i_seek = NO_SEEK;

  addhash_inode(rip);

  return(rip);
}

/*===========================================================================*
 *                find_inode                                                 *
 *===========================================================================*/
struct inode *find_inode(dev_t dev, ino_t numb)
{
/* Find the inode specified by the inode and device number. */
  struct inode *rip;
  int hashi;

  hashi = (int) (numb & INODE_HASH_MASK);

  LIST_FOREACH(rip, &hash_inodes[hashi], i_hash) {
	if (rip->i_count > 0 && rip->i_num == numb && rip->i_dev == dev)
		return(rip);
  }

  return(NULL);
}

/*===========================================================================*
 *                put_inode                                                  *
 *===========================================================================*/
void put_inode(struct inode *rip)
{
/* The caller is no longer using this inode.  If no one else is using it
 * either, write it back to the disk if it is dirty.  If it has no links left,
 * truncate it, free its blocks, and return the inode to the free pool.
 */
  int freed = FALSE;

  if (rip == NULL)
	return;		/* checking here is easier than in the caller */

  if (rip->i_count < 1)
	panic("put_inode: i_count already below 1: %d", rip->i_count);

  if (--rip->i_count == 0) {	/* no one is using it now */
	if (rip->i_din.di_nlink == NO_LINK && !rip->i_sp->s_rd_only) {
		/* No links remain: release all blocks and free the inode. */
		(void) truncate_inode(rip, (off_t) 0);
		free_inode(rip);	/* clears di_*, frees the cg inode bit */
		freed = TRUE;
	}

	rip->i_mountpoint = FALSE;
	if (rip->i_dirt == IN_DIRTY)
		rw_inode(rip, WRITING);

	if (freed) {
		/* Return it to the free list. */
		unhash_inode(rip);
		rip->i_num = NO_ENTRY;
		TAILQ_INSERT_HEAD(&unused_inodes, rip, i_unused);
	} else {
		/* Unused: put at the back of the LRU list (cache it). */
		TAILQ_INSERT_TAIL(&unused_inodes, rip, i_unused);
	}
  }
}

/*===========================================================================*
 *                update_times                                               *
 *===========================================================================*/
void update_times(struct inode *rip)
{
/* Fill in the atime, ctime and mtime fields that have been marked for update.
 * Skipped for read-only file systems.
 */
  time_t cur_time;
  struct super_block *sp;

  sp = rip->i_sp;
  if (sp->s_rd_only)
	return;

  cur_time = clock_time(NULL);
  if (rip->i_update & ATIME) {
	rip->i_din.di_atime = cur_time;
	rip->i_din.di_atimensec = 0;
  }
  if (rip->i_update & CTIME) {
	rip->i_din.di_ctime = cur_time;
	rip->i_din.di_ctimensec = 0;
  }
  if (rip->i_update & MTIME) {
	rip->i_din.di_mtime = cur_time;
	rip->i_din.di_mtimensec = 0;
  }
  rip->i_update = 0;
}

/*===========================================================================*
 *                ufs1_to_ufs2 / ufs2_to_ufs1                                *
 *===========================================================================*/
/* The in-core inode always holds a (wide) UFS2 dinode.  On a UFS1 file system
 * the on-disk inode is the narrower struct ufs1_dinode (32-bit block pointers
 * and times), so rw_inode widens it on read and narrows it back on write.
 *
 * Inline ("fast") symlinks store the target bytes in the block-pointer area
 * rather than block numbers; those bytes are copied verbatim, since a numeric
 * widen/narrow of di_db[]/di_ib[] would corrupt the string.  Every other file
 * type stores block numbers, converted per element (block numbers are small
 * non-negative frag numbers, so the widening is loss-free). */
static int is_shortlink(const struct fs *fs, u_int16_t mode, u_int64_t size,
	u_int64_t blocks)
{
  return ((mode & IFMT) == IFLNK &&
	size < (u_int64_t) fs->fs_maxsymlinklen && blocks == 0);
}

/*===========================================================================*
 *                inline_symlink                                             *
 *===========================================================================*/
int inline_symlink(const struct inode *rip)
{
/* Does this inode keep its symlink target in the block-pointer area instead of
 * block numbers?  Anything that walks di_db[]/di_ib[] must ask first.  This is
 * the same test the on-disk conversion above makes, so the two cannot disagree
 * about what those bytes mean.
 */
  return is_shortlink(&rip->i_sp->s_fs, rip->i_din.di_mode,
	rip->i_din.di_size, rip->i_din.di_blocks);
}

static void ufs1_to_ufs2(const struct fs *fs, const struct ufs1_dinode *d1,
	struct ufs2_dinode *d2)
{
  int i;

  memset(d2, 0, sizeof(*d2));
  d2->di_mode = d1->di_mode;
  d2->di_nlink = d1->di_nlink;
  d2->di_uid = d1->di_uid;
  d2->di_gid = d1->di_gid;
  d2->di_size = d1->di_size;
  d2->di_blocks = d1->di_blocks;
  d2->di_atime = d1->di_atime;
  d2->di_atimensec = d1->di_atimensec;
  d2->di_mtime = d1->di_mtime;
  d2->di_mtimensec = d1->di_mtimensec;
  d2->di_ctime = d1->di_ctime;
  d2->di_ctimensec = d1->di_ctimensec;
  d2->di_flags = d1->di_flags;
  d2->di_gen = d1->di_gen;
  d2->di_modrev = d1->di_modrev;

  if (is_shortlink(fs, d1->di_mode, d1->di_size, d1->di_blocks)) {
	memcpy(d2->di_db, d1->di_db, UFS1_MAXSYMLINKLEN);
  } else {
	for (i = 0; i < UFS_NDADDR; i++)
		d2->di_db[i] = d1->di_db[i];
	for (i = 0; i < UFS_NIADDR; i++)
		d2->di_ib[i] = d1->di_ib[i];
  }
}

static void ufs2_to_ufs1(const struct fs *fs, const struct ufs2_dinode *d2,
	struct ufs1_dinode *d1)
{
  int i;

  memset(d1, 0, sizeof(*d1));
  d1->di_mode = d2->di_mode;
  d1->di_nlink = d2->di_nlink;
  d1->di_uid = d2->di_uid;
  d1->di_gid = d2->di_gid;
  d1->di_size = d2->di_size;
  d1->di_blocks = (u_int32_t) d2->di_blocks;
  d1->di_atime = (int32_t) d2->di_atime;
  d1->di_atimensec = d2->di_atimensec;
  d1->di_mtime = (int32_t) d2->di_mtime;
  d1->di_mtimensec = d2->di_mtimensec;
  d1->di_ctime = (int32_t) d2->di_ctime;
  d1->di_ctimensec = d2->di_ctimensec;
  d1->di_flags = d2->di_flags;
  d1->di_gen = d2->di_gen;
  d1->di_modrev = d2->di_modrev;

  if (is_shortlink(fs, d2->di_mode, d2->di_size, d2->di_blocks)) {
	memcpy(d1->di_db, d2->di_db, UFS1_MAXSYMLINKLEN);
  } else {
	for (i = 0; i < UFS_NDADDR; i++)
		d1->di_db[i] = (int32_t) d2->di_db[i];
	for (i = 0; i < UFS_NIADDR; i++)
		d1->di_ib[i] = (int32_t) d2->di_ib[i];
  }
}

/*===========================================================================*
 *                rw_inode                                                   *
 *===========================================================================*/
void rw_inode(struct inode *rip, int rw_flag)
{
/* An entry in the inode table is to be copied to or from the disk. */
  struct buf *bp;
  struct super_block *sp;
  struct fs *fs;
  char *diskino;
  block64_t fsba;
  off_t byteoff;
  unsigned int fragidx, inoff, dsize;
  int is_ufs1;

  sp = get_super(rip->i_dev);
  rip->i_sp = sp;
  fs = &sp->s_fs;
  is_ufs1 = (fs->fs_magic == FS_UFS1_MAGIC);
  dsize = is_ufs1 ? (unsigned int) DINODE1_SIZE : (unsigned int) DINODE2_SIZE;

  /* Locate the fragment of the inode block that holds this inode, and the
   * byte offset of the inode within that fragment.  A full inode block spans
   * fs_frag fragments; the cache addresses fragments individually.
   */
  fsba = (block64_t) ino_to_fsba(fs, rip->i_num);
  byteoff = (off_t) ino_to_fsbo(fs, rip->i_num) * (off_t) dsize;
  fragidx = (unsigned int) (byteoff / fs->fs_fsize);
  inoff = (unsigned int) (byteoff % fs->fs_fsize);

  bp = get_block(rip->i_dev, fsba + fragidx, NORMAL);
  diskino = b_data(bp) + inoff;

  if (rw_flag == WRITING) {
	if (rip->i_update)
		update_times(rip);
	if (sp->s_rd_only == FALSE) {
		if (is_ufs1)
			ufs2_to_ufs1(fs, &rip->i_din,
				(struct ufs1_dinode *) diskino);
		else
			memcpy(diskino, &rip->i_din,
				sizeof(struct ufs2_dinode));
		lmfs_markdirty(bp);
	}
  } else {
	if (is_ufs1)
		ufs1_to_ufs2(fs, (struct ufs1_dinode *) diskino, &rip->i_din);
	else
		memcpy(&rip->i_din, diskino, sizeof(struct ufs2_dinode));
  }

  put_block(bp);
  rip->i_dirt = IN_CLEAN;
}

/*===========================================================================*
 *                dup_inode                                                  *
 *===========================================================================*/
void dup_inode(struct inode *ip)
{
/* Simplified form of get_inode() for when the inode pointer is already known. */
  ip->i_count++;
}

/*===========================================================================*
 *                fs_seek                                                    *
 *===========================================================================*/
void fs_seek(ino_t ino_nr)
{
/* A seek was made on an open file; record it so read-ahead heuristics know the
 * next access may not be sequential.
 */
  struct inode *rip;

  if ((rip = find_inode(fs_dev, ino_nr)) != NULL)
	rip->i_seek = ISEEK;
}
