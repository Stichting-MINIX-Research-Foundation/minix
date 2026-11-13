/* Inode table.  This table holds inodes that are currently in use.  In some
 * cases they have been opened by an open() or creat() system call, in other
 * cases the file system itself needs the inode to, for example, search a
 * directory for a path name.
 *
 * The in-core inode always holds a (wide) UFS2 dinode, reached as
 * rip->i_din.di_*.  On a UFS1 file system the narrower on-disk struct
 * ufs1_dinode is converted to/from this form in rw_inode() (inode.c).
 */

#ifndef FFS_INODE_H
#define FFS_INODE_H

#include <sys/queue.h>

EXTERN struct inode {
  struct ufs2_dinode i_din;	/* the on-disk inode, read natively */

  /* The following items are not present on the disk. */
  dev_t i_dev;			/* which device is the inode on */
  ino_t i_num;			/* inode number on its (minor) device */
  int i_count;			/* # times inode used; 0 means slot is free */
  struct super_block *i_sp;	/* pointer to super block for inode's device */
  char i_dirt;			/* CLEAN or DIRTY */

  off_t i_last_dpos;		/* where to start dentry search */
  int i_last_dentry_size;	/* size of last found dentry */

  char i_mountpoint;		/* true if mounted on */
  char i_seek;			/* set on LSEEK, cleared on READ/WRITE */
  char i_update;		/* the ATIME, CTIME, and MTIME bits are here */

  LIST_ENTRY(inode) i_hash;	/* hash list */
  TAILQ_ENTRY(inode) i_unused;	/* free and unused list */
} inode[NR_INODES];

/* list of unused/free inodes */
EXTERN TAILQ_HEAD(unused_inodes_t, inode) unused_inodes;

/* inode hashtable */
EXTERN LIST_HEAD(inodelist, inode) hash_inodes[INODE_HASH_SIZE];

/* Values for i_seek. */
#define NO_SEEK            0    /* i_seek = NO_SEEK if last op was not SEEK */
#define ISEEK              1    /* i_seek = ISEEK if last op was SEEK */

#endif /* FFS_INODE_H */
