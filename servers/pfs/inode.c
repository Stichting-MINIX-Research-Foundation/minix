/* This file manages the inode table.  There are procedures to allocate and
 * deallocate inodes, acquire, erase, and release them, and read and write
 * them from the disk.
 *
 * The entry points into this file are
 *   get_inode:	   search inode table for a given inode; if not there,
 *                 read it
 *   put_inode:	   indicate that an inode is no longer needed in memory
 *   alloc_inode:  allocate a new, unused inode
 *   wipe_inode:   erase some fields of a newly allocated inode
 *   free_inode:   mark an inode as available for a new file
 *   update_times: update atime, ctime, and mtime
 *   find_inode:   retrieve pointer to inode in inode cache
 *
 */

#include "fs.h"
#include "buf.h"
#include "inode.h"
#include <minix/vfsif.h>

static void addhash_inode(struct inode * const node);
static void unhash_inode(struct inode * const node);


/*===========================================================================*
 *				fs_putnode				     *
 *===========================================================================*/
int fs_putnode(message *fs_m_in, message *fs_m_out)
{
/* Find the inode specified by the request message and decrease its counter.*/

  struct inode *rip;
  int count;
  dev_t dev;
  ino_t inum;

  rip = find_inode(fs_m_in->m_vfs_fs_putnode.inode);

  if(!rip) {
	  printf("%s:%d put_inode: inode #%llu not found\n", __FILE__,
		 __LINE__, fs_m_in->m_vfs_fs_putnode.inode);
	  panic("fs_putnode failed");
  }

  count = fs_m_in->m_vfs_fs_putnode.count;
  if (count <= 0) {
	printf("%s:%d put_inode: bad value for count: %d\n", __FILE__,
	       __LINE__, count);
	panic("fs_putnode failed");
  } else if(count > rip->i_count) {
	printf("%s:%d put_inode: count too high: %d > %d\n", __FILE__,
	       __LINE__, count, rip->i_count);
	panic("fs_putnode failed");
  }

  /* Decrease reference counter, but keep one reference; it will be consumed by
   * put_inode(). */
  rip->i_count -= count - 1;
  dev = rip->i_dev;
  inum = rip->i_num;
  put_inode(rip);
  if (rip->i_count == 0) put_block(dev, inum);
  return(OK);
}


/*===========================================================================*
 *				init_inode_cache			     *
 *===========================================================================*/
void init_inode_cache()
{
  struct inode *rip;
  struct inodelist *rlp;

  /* init free/unused list */
  TAILQ_INIT(&unused_inodes);

  /* init hash lists */
  for (rlp = &hash_inodes[0]; rlp < &hash_inodes[INODE_HASH_SIZE]; ++rlp)
      LIST_INIT(rlp);

  /* add free inodes to unused/free list */
  for (rip = &inode[0]; rip < &inode[PFS_NR_INODES]; ++rip) {
      rip->i_num = NO_ENTRY;
      TAILQ_INSERT_HEAD(&unused_inodes, rip, i_unused);
  }

  /* Reserve the first inode (bit 0) to prevent it from being allocated later*/
  if (alloc_bit() != NO_BIT) printf("PFS could not reserve NO_BIT\n");
  busy = 0; /* This bit does not make the server 'in use/busy'. */
}


/*===========================================================================*
 *				addhash_inode   			     *
 *===========================================================================*/
static void addhash_inode(struct inode * const node)
{
  int hashi = (int) (node->i_num & INODE_HASH_MASK);

  /* insert into hash table */
  LIST_INSERT_HEAD(&hash_inodes[hashi], node, i_hash);
}


/*===========================================================================*
 *				unhash_inode      			     *
 *===========================================================================*/
static void unhash_inode(struct inode * const node)
{
  /* remove from hash table */
  LIST_REMOVE(node, i_hash);
}


/*===========================================================================*
 *				get_inode				     *
 *===========================================================================*/
struct inode *get_inode(
  dev_t dev,		/* device on which inode resides */
  ino_t numb		/* inode number */
)
{
/* Find the inode in the hash table. If it is not there, get a free inode
 * load it from the disk if it's necessary and put on the hash list
 */
  register struct inode *rip;
  int hashi;

  hashi = (int) (numb & INODE_HASH_MASK);

  /* Search inode in the hash table */
  LIST_FOREACH(rip, &hash_inodes[hashi], i_hash) {
	if (rip->i_num == numb && rip->i_dev == dev) {
		/* If unused, remove it from the unused/free list */
		if (rip->i_count == 0) {
			TAILQ_REMOVE(&unused_inodes, rip, i_unused);
		}
		++rip->i_count;

		return(rip);
	}
  }

  /* Inode is not on the hash, get a free one */
  if (TAILQ_EMPTY(&unused_inodes)) {
      err_code = ENFILE;
      return(NULL);
  }
  rip = TAILQ_FIRST(&unused_inodes);

  /* If not free unhash it */
  if (rip->i_num != NO_ENTRY) unhash_inode(rip);

  /* Inode is not unused any more */
  TAILQ_REMOVE(&unused_inodes, rip, i_unused);

  /* Load the inode. */
  rip->i_dev = dev;
  rip->i_num = numb;
  rip->i_count = 1;
  rip->i_update = 0;		/* all the times are initially up-to-date */

  /* Add to hash */
  addhash_inode(rip);


  return(rip);
}


/*===========================================================================*
 *				find_inode        			     *
 *===========================================================================*/
struct inode *find_inode(ino_t numb	/* inode number */)
{
/* Find the inode specified by the inode and device number.
 */
  struct inode *rip;
  int hashi;

  hashi = (int) (numb & INODE_HASH_MASK);

  /* Search inode in the hash table */
  LIST_FOREACH(rip, &hash_inodes[hashi], i_hash) {
      if (rip->i_count > 0 && rip->i_num == numb) {
          return(rip);
      }
  }

  return(NULL);
}


/*===========================================================================*
 *				put_inode				     *
 *===========================================================================*/
void put_inode(rip)
struct inode *rip;	/* pointer to inode to be released */
{
/* The caller is no longer using this inode.  If no one else is using it either
 * write it back to the disk immediately.  If it has no links, truncate it and
 * return it to the pool of available inodes.
 */

  if (rip == NULL) return;	/* checking here is easier than in caller */

  if (rip->i_count < 1)
	panic("put_inode: i_count already below 1: %d", rip->i_count);

  if (--rip->i_count == 0) {	/* i_count == 0 means no one is using it now */
	if (rip->i_nlinks == NO_LINK) { /* Are there links to this file? */
		/* no links, free the inode. */
		truncate_inode(rip, 0);	/* return all the disk blocks */
		rip->i_mode = I_NOT_ALLOC;	/* clear I_TYPE field */
		free_inode(rip);
	} else {
		truncate_inode(rip, (off_t) 0);
	}

	if (rip->i_nlinks == NO_LINK) {
		/* free, put at the front of the LRU list */
		unhash_inode(rip);
		rip->i_num = NO_ENTRY;
		rip->i_dev = NO_DEV;
		rip->i_rdev = NO_DEV;
		TAILQ_INSERT_HEAD(&unused_inodes, rip, i_unused);
	} else {
		/* unused, put at the back of the LRU (cache it) */
		TAILQ_INSERT_TAIL(&unused_inodes, rip, i_unused);
	}
  }
}


/*===========================================================================*
 *				alloc_inode				     *
 *===========================================================================*/
struct inode *alloc_inode(dev_t dev, mode_t bits, uid_t uid, gid_t gid)
{
/* Allocate a free inode on 'dev', and return a pointer to it. */

  register struct inode *rip;
  bit_t b;
  ino_t i_num;
  int print_oos_msg = 1;

  b = alloc_bit();
  if (b == NO_BIT) {
	err_code = ENOSPC;
	if (print_oos_msg)
		printf("PipeFS is out of inodes\n");
	print_oos_msg = 0;	/* Don't repeat message */
	return(NULL);
  }
  i_num = (ino_t) b;
  print_oos_msg = 1;


  /* Try to acquire a slot in the inode table. */
  if ((rip = get_inode(dev, i_num)) == NULL) {
	/* No inode table slots available.  Free the inode if just allocated.*/
	if (dev == NO_DEV) free_bit(b);
  } else {
	/* An inode slot is available. */

	rip->i_mode = bits;		/* set up RWX bits */
	rip->i_nlinks = NO_LINK;	/* initial no links */
	rip->i_uid = uid;		/* set file user id */
	rip->i_gid = gid;		/* ditto group id */

	/* Fields not cleared already are cleared in wipe_inode().  They have
	 * been put there because truncate() needs to clear the same fields if
	 * the file happens to be open while being truncated.  It saves space
	 * not to repeat the code twice.
	 */
	wipe_inode(rip);
  }

  return(rip);
}


/*===========================================================================*
 *				wipe_inode				     *
 *===========================================================================*/
void wipe_inode(rip)
struct inode *rip;	/* the inode to be erased */
{
/* Erase some fields in the inode.  This function is called from alloc_inode()
 * when a new inode is to be allocated, and from truncate(), when an existing
 * inode is to be truncated.
 */

  rip->i_size = 0;
  rip->i_update = ATIME | CTIME | MTIME;	/* update all times later */
}


/*===========================================================================*
 *				free_inode				     *
 *===========================================================================*/
void free_inode(rip)
struct inode *rip;
{
/* Return an inode to the pool of unallocated inodes. */

  bit_t b;

  if (rip->i_num <= 0 || rip->i_num >= PFS_NR_INODES) return;
  b = (bit_t) rip->i_num;
  free_bit(b);
}


/*===========================================================================*
 *				update_times				     *
 *===========================================================================*/
void update_times(rip)
struct inode *rip;	/* pointer to inode to be read/written */
{
/* Various system calls are required by the standard to update atime, ctime,
 * or mtime.  Since updating a time requires sending a message to the clock
 * task--an expensive business--the times are marked for update by setting
 * bits in i_update.  When a stat, fstat, or sync is done, or an inode is
 * released, update_times() may be called to actually fill in the times.
 */

  time_t cur_time;

  cur_time = clock_time();
  if (rip->i_update & ATIME) rip->i_atime = cur_time;
  if (rip->i_update & CTIME) rip->i_ctime = cur_time;
  if (rip->i_update & MTIME) rip->i_mtime = cur_time;
  rip->i_update = 0;		/* they are all up-to-date now */
}
