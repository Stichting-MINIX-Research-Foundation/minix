#ifndef _MKFS_MFS_CONST_H__
#define _MKFS_MFS_CONST_H__

/* Tables sizes */
#define NR_DZONES       7	/* # direct zone numbers in a V2 inode */
#define NR_TZONES      10	/* total # zone numbers in a V2 inode */

/* Blocks are of a fixed size */
#define MFS_STATIC_BLOCK_SIZE	1024

#define SUPER_BLOCK_BYTES  MFS_STATIC_BLOCK_SIZE	/* 1 block */

/* The type of sizeof may be (unsigned) long.  Use the following macro for
 * taking the sizes of small objects so that there are no surprises like
 * (small) long constants being passed to routines expecting an int.
 */
#define usizeof(t) ((unsigned) sizeof(t))

/* File system types: magic number contained in super-block. */
#define SUPER_V2	0x2468	/* magic # for V2 file systems */
#define SUPER_MAGIC	SUPER_V2

/* Miscellaneous constants */
#define SU_UID		((uid_t) 0)	/* super_user's uid_t */
#define SECTOR_SIZE	512

#define BOOT_BLOCK	((block_t) 0)	/* block number of boot block */
#define SUPER_BLOCK	((block_t) 1)	/* block number of super block */
#define START_BLOCK	((block_t) 2)	/* first block of FS (not counting SB) */

#define ROOT_INODE	((ino_t) 1)	/* inode number for root directory */

/* Derived sizes pertaining to the file system. */
#define FS_BITMAP_CHUNKS(b) ((b)/usizeof (uint32_t))/* # map chunks/blk   */
#define FS_BITCHUNK_BITS		(usizeof(uint32_t) * CHAR_BIT)
#define FS_BITS_PER_BLOCK(b)	(FS_BITMAP_CHUNKS(b) * FS_BITCHUNK_BITS)

#define ZONE_NUM_SIZE		usizeof (zone_t)  /* # bytes in zone */
#define INODE_SIZE		usizeof (struct inode)  /* bytes in dsk ino */
#define INODES_PER_BLOCK(b)	((b)/INODE_SIZE)  /* # V2 dsk inodes/blk */
#define INDIRECTS(b)		((b)/ZONE_NUM_SIZE)  /* # zones/indir block */

#define DIR_ENTRY_SIZE		usizeof(struct direct) /* # bytes/dir entry */
#define NR_DIR_ENTRIES(b)	((b)/DIR_ENTRY_SIZE)  /* # dir entries/blk  */
#endif
