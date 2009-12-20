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
 *   dup_inode:	   indicate that someone else is using an inode table entry
 *   find_inode:   retrieve pointer to inode in inode cache
 *
 */

#include "fs.h"
#include "buf.h"
#include "inode.h"
#include <minix/vfsif.h>

FORWARD _PROTOTYPE( int addhash_inode, (struct inode *node)		); 
FORWARD _PROTOTYPE( int unhash_inode, (struct inode *node) 		);


/*===========================================================================*
 *				fs_putnode				     *
 *===========================================================================*/
PUBLIC int fs_putnode()
{
/* Find the inode specified by the request message and decrease its counter.*/

  struct inode *rip;
  int count;
  dev_t dev;
  ino_t inum;
  
  rip = find_inode(fs_m_in.REQ_INODE_NR);

  if(!rip) {
	  printf("%s:%d put_inode: inode #%d dev: %d not found\n", __FILE__,
		 __LINE__, fs_m_in.REQ_INODE_NR, fs_m_in.REQ_DEV);
	  panic(__FILE__, "fs_putnode failed", NO_NUM);
  }

  count = fs_m_in.REQ_COUNT;
  if (count <= 0) {
	printf("%s:%d put_inode: bad value for count: %d\n", __FILE__,
	       __LINE__, count);
	panic(__FILE__, "fs_putnode failed", NO_NUM);
  } else if(count > rip->i_count) {
	printf("%s:%d put_inode: count too high: %d > %d\n", __FILE__,
	       __LINE__, count, rip->i_count);
	panic(__FILE__, "fs_putnode failed", NO_NUM);
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
PUBLIC void init_inode_cache()
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
      rip->i_num = 0;
      TAILQ_INSERT_HEAD(&unused_inodes, rip, i_unused);
  }

  /* Reserve the first inode (bit 0) to prevent it from being allocated later*/
  if (alloc_bit() != NO_BIT) printf("PFS could not reserve NO_BIT\n");
  busy = 0; /* This bit does not make the server 'in use/busy'. */
}


/*===========================================================================*
 *				addhash_inode   			     *
 *===========================================================================*/
PRIVATE int addhash_inode(struct inode *node) 
{
  int hashi = node->i_num & INODE_HASH_MASK;
  
  /* insert into hash table */
  LIST_INSERT_HEAD(&hash_inodes[hashi], node, i_hash);
  return(OK);
}


/*===========================================================================*
 *				unhash_inode      			     *
 *===========================================================================*/
PRIVATE int unhash_inode(struct inode *node) 
{
  /* remove from hash table */
  LIST_REMOVE(node, i_hash);
  return(OK);
}


/*===========================================================================*
 *				get_inode				     *
 *===========================================================================*/
PUBLIC struct inode *get_inode(dev, numb)
dev_t dev;			/* device on which inode resides */
int numb;			/* inode number (ANSI: may not be unshort) */
{
/* Find the inode in the hash table. If it is not there, get a free inode
 * load it from the disk if it's necessary and put on the hash list 
 */
  register struct inode *rip, *xp;
  int hashi;

  hashi = numb & INODE_HASH_MASK;

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
      return(NIL_INODE);
  }
  rip = TAILQ_FIRST(&unused_inodes);

  /* If not free unhash it */
  if (rip->i_num != 0) unhash_inode(rip);
  
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
PUBLIC struct inode *find_inode(numb)
int numb;			/* inode number (ANSI: may not be unshort) */
{
/* Find the inode specified by the inode and device number.
 */
  struct inode *rip;
  int hashi;

  hashi = numb & INODE_HASH_MASK;

  /* Search inode in the hash table */
  LIST_FOREACH(rip, &hash_inodes[hashi], i_hash) {
      if (rip->i_count > 0 && rip->i_num == numb) {
          return(rip);
      }
  }
  
  return(NIL_INODE);
}


/*===========================================================================*
 *				put_inode				     *
 *===========================================================================*/
PUBLIC void put_inode(rip)
register struct inode *rip;	/* pointer to inode to be released */
{
/* The caller is no longer using this inode.  If no one else is using it either
 * write it back to the disk immediately.  If it has no links, truncate it and
 * return it to the pool of available inodes.
 */

  if (rip == NIL_INODE) return;	/* checking here is easier than in caller */

  if (rip->i_count < 1)
	panic(__FILE__, "put_inode: i_count already below 1", rip->i_count);

  if (--rip->i_count == 0) {	/* i_count == 0 means no one is using it now */
	if (rip->i_nlinks == 0) {
		/* i_nlinks == 0 means free the inode. */
		truncate_inode(rip, 0);	/* return all the disk blocks */
		rip->i_mode = I_NOT_ALLOC;	/* clear I_TYPE field */
		free_inode(rip);
	} else {
		truncate_inode(rip, 0);
	}

	if (rip->i_nlinks == 0) {
		/* free, put at the front of the LRU list */
		unhash_inode(rip);
		rip->i_num = 0;
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
PUBLIC struct inode *alloc_inode(dev_t dev, mode_t bits)
{
/* Allocate a free inode on 'dev', and return a pointer to it. */

  register struct inode *rip;
  int major, minor;
  bit_t b;
  ino_t i_num;

  b = alloc_bit();
  if (b == NO_BIT) {
  	err_code = ENOSPC;
  	printf("PipeFS is out of inodes\n");
  	return(NIL_INODE);
  }
  i_num = (ino_t) b;
  

  /* Try to acquire a slot in the inode table. */
  if ((rip = get_inode(dev, i_num)) == NIL_INODE) {
	/* No inode table slots available.  Free the inode if just allocated.*/
	if (dev == NO_DEV) free_bit(b);
  } else {
	/* An inode slot is available. */

	rip->i_mode = bits;		/* set up RWX bits */
	rip->i_nlinks = 0;		/* initial no links */
	rip->i_uid = caller_uid;	/* file's uid is owner's */
	rip->i_gid = caller_gid;	/* ditto group id */

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
PUBLIC void wipe_inode(rip)
register struct inode *rip;	/* the inode to be erased */
{
/* Erase some fields in the inode.  This function is called from alloc_inode()
 * when a new inode is to be allocated, and from truncate(), when an existing
 * inode is to be truncated.
 */

  register int i;

  rip->i_size = 0;
  rip->i_update = ATIME | CTIME | MTIME;	/* update all times later */
}


/*===========================================================================*
 *				free_inode				     *
 *===========================================================================*/
PUBLIC void free_inode(rip)
struct inode *rip;
{
/* Return an inode to the pool of unallocated inodes. */

  bit_t b;

  if (rip->i_num <= 0 || rip->i_num >= NR_INODES) return;
  b = rip->i_num;
  free_bit(b);
}


/*===========================================================================*
 *				dup_inode				     *
 *===========================================================================*/
PUBLIC void dup_inode(ip)
struct inode *ip;		/* The inode to be duplicated. */
{
/* This routine is a simplified form of get_inode() for the case where
 * the inode pointer is already known.
 */

  ip->i_count++;
}


/*===========================================================================*
 *				update_times				     *
 *===========================================================================*/
PUBLIC void update_times(rip)
register struct inode *rip;	/* pointer to inode to be read/written */
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

