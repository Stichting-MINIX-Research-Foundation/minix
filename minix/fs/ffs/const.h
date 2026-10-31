#ifndef FFS_CONST_H
#define FFS_CONST_H

/* Table sizes. */
#define NR_INODES        512    /* # slots in "in core" inode table; should be
				 * more or less the same as NR_VNODES in vfs */

#define INODE_HASH_LOG2   7     /* 2-based log of the inode hash size */
#define INODE_HASH_SIZE   ((unsigned long)1<<INODE_HASH_LOG2)
#define INODE_HASH_MASK   (((unsigned long)1<<INODE_HASH_LOG2)-1)

/* Miscellaneous constants. */
#define SU_UID          ((uid_t) 0)     /* super_user's uid_t */
#define NORMAL          0               /* forces get_block to do disk read */
#define NO_READ         1               /* prevents get_block from reading */
#define PREFETCH        2               /* tells get_block not to read/mark */

/* NO_ENTRY, NO_BLOCK and NO_DEV come from <minix/const.h>; a hole or an
 * unused slot is value 0 in both the 32-bit block_t and our 64-bit block64_t.
 */

#define LOOK_UP            0    /* tells search_dir to look up a string */
#define ENTER              1    /* tells search_dir to make a dir entry */
#define DELETE             2    /* tells search_dir to delete an entry */
#define IS_EMPTY           3    /* tells search_dir to test for emptiness */

#define IN_CLEAN           0    /* inode disk and memory copies identical */
#define IN_DIRTY           1    /* inode disk and memory copies differ */
#define ATIME            002    /* set if atime field needs updating */
#define CTIME            004    /* set if ctime field needs updating */
#define MTIME            010    /* set if mtime field needs updating */

#define ROOT_INODE      UFS_ROOTINO     /* inode number of the root directory */

/* NO_LINK (di_nlink == 0 means a free inode) comes from <minix/const.h>. */

#define FFS_NAME_MAX	FFS_MAXNAMLEN

#endif /* FFS_CONST_H */
