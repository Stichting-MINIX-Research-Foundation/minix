#ifndef __PFS_INODE_H__
#define __PFS_INODE_H__

/* Inode table.  This table holds inodes that are currently in use.
 */

#include <sys/queue.h>

EXTERN struct inode {
  mode_t i_mode;		/* file type, protection, etc. */
  nlink_t i_nlinks;		/* how many links to this file */
  uid_t i_uid;			/* user id of the file's owner */
  gid_t i_gid;			/* group number */
  off_t i_size;			/* current file size in bytes */
  time_t i_atime;		/* time of last access (V2 only) */
  time_t i_mtime;		/* when was file data last changed */
  time_t i_ctime;		/* when was inode itself changed (V2 only)*/

  /* The following items are not present on the disk. */
  dev_t i_dev;			/* which device is the inode on */
  dev_t i_rdev;			/* which special device is the inode on */
  ino_t i_num;			/* inode number on its (minor) device */
  int i_count;			/* # times inode used; 0 means slot is free */
  char i_update;		/* the ATIME, CTIME, and MTIME bits are here */

  LIST_ENTRY(inode) i_hash;     /* hash list */
  TAILQ_ENTRY(inode) i_unused;  /* free and unused list */


} inode[PFS_NR_INODES];

/* list of unused/free inodes */
EXTERN TAILQ_HEAD(unused_inodes_t, inode)  unused_inodes;

/* inode hashtable */
EXTERN LIST_HEAD(inodelist, inode)         hash_inodes[INODE_HASH_SIZE];


#endif
