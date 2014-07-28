#ifndef __MFS_INODE_H__
#define __MFS_INODE_H__

/* Inode table.  This table holds inodes that are currently in use.  In some
 * cases they have been opened by an open() or creat() system call, in other
 * cases the file system itself needs the inode for one reason or another,
 * such as to search a directory for a path name.
 * The first part of the struct holds fields that are present on the
 * disk; the second part holds fields not present on the disk.
 *
 * Updates:
 * 2007-01-06: jfdsmit@gmail.com added i_zsearch
 */

#include <sys/queue.h>
#include <minix/vfsif.h>

#include "super.h"

EXTERN struct inode {
  u16_t i_mode;		/* file type, protection, etc. */
  u16_t i_nlinks;		/* how many links to this file */
  u16_t i_uid;			/* user id of the file's owner */
  u16_t i_gid;			/* group number */
  i32_t i_size;			/* current file size in bytes */
  u32_t i_atime;		/* time of last access (V2 only) */
  u32_t i_mtime;		/* when was file data last changed */
  u32_t i_ctime;		/* when was inode itself changed (V2 only)*/
  u32_t i_zone[V2_NR_TZONES]; /* zone numbers for direct, ind, and dbl ind */
  
  /* The following items are not present on the disk. */
  dev_t i_dev;			/* which device is the inode on */
  ino_t i_num;			/* inode number on its (minor) device */
  int i_count;			/* # times inode used; 0 means slot is free */
  unsigned int i_ndzones;	/* # direct zones (Vx_NR_DZONES) */
  unsigned int i_nindirs;	/* # indirect zones per indirect block */
  struct super_block *i_sp;	/* pointer to super block for inode's device */
  char i_dirt;			/* CLEAN or DIRTY */
  zone_t i_zsearch;		/* where to start search for new zones */
  off_t i_last_dpos;		/* where to start dentry search */
  
  char i_mountpoint;		/* true if mounted on */

  char i_seek;			/* set on LSEEK, cleared on READ/WRITE */
  char i_update;		/* the ATIME, CTIME, and MTIME bits are here */

  LIST_ENTRY(inode) i_hash;     /* hash list */
  TAILQ_ENTRY(inode) i_unused;  /* free and unused list */
  
} inode[NR_INODES];

/* list of unused/free inodes */ 
EXTERN TAILQ_HEAD(unused_inodes_t, inode)  unused_inodes;

/* inode hashtable */
EXTERN LIST_HEAD(inodelist, inode)         hash_inodes[INODE_HASH_SIZE];

EXTERN unsigned int inode_cache_hit;
EXTERN unsigned int inode_cache_miss;


/* Field values.  Note that CLEAN and DIRTY are defined in "const.h" */
#define NO_SEEK            0	/* i_seek = NO_SEEK if last op was not SEEK */
#define ISEEK              1	/* i_seek = ISEEK if last op was SEEK */

#define IN_MARKCLEAN(i) i->i_dirt = IN_CLEAN
#define IN_MARKDIRTY(i) do { if(i->i_sp->s_rd_only) { printf("%s:%d: dirty inode on rofs ", __FILE__, __LINE__); util_stacktrace(); } else { i->i_dirt = IN_DIRTY; } } while(0)

#define IN_ISCLEAN(i) i->i_dirt == IN_CLEAN
#define IN_ISDIRTY(i) i->i_dirt == IN_DIRTY

#endif
