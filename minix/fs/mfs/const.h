#ifndef __MFS_CONST_H__
#define __MFS_CONST_H__

/* Tables sizes */
#define V2_NR_DZONES       7	/* # direct zone numbers in a V2 inode */
#define V2_NR_TZONES      10	/* total # zone numbers in a V2 inode */

#define NR_INODES        512	/* # slots in "in core" inode table,
				 * should be more or less the same as
				 * NR_VNODES in vfs
				 */

#define INODE_HASH_LOG2   7     /* 2 based logarithm of the inode hash size */
#define INODE_HASH_SIZE   ((unsigned long)1<<INODE_HASH_LOG2)
#define INODE_HASH_MASK   (((unsigned long)1<<INODE_HASH_LOG2)-1)

/* Max. filename length */
#define MFS_NAME_MAX	 MFS_DIRSIZ


/* File system types. */
#define SUPER_MAGIC   0x137F	/* magic number contained in super-block */
#define SUPER_REV     0x7F13	/* magic # when 68000 disk read on PC or vv */
#define SUPER_V2      0x2468	/* magic # for V2 file systems */
#define SUPER_V2_REV  0x6824	/* V2 magic written on PC, read on 68K or vv */
#define SUPER_V3      0x4d5a	/* magic # for V3 file systems */

#define V2		   2	/* version number of V2 file systems */ 
#define V3		   3	/* version number of V3 file systems */ 

/* Miscellaneous constants */
#define NO_BIT   ((bit_t) 0)	/* returned by alloc_bit() to signal failure */

#define LOOK_UP            0 /* tells search_dir to lookup string */
#define ENTER              1 /* tells search_dir to make dir entry */
#define DELETE             2 /* tells search_dir to delete entry */
#define IS_EMPTY           3 /* tells search_dir to ret. OK or ENOTEMPTY */  

/* write_map() args */
#define WMAP_FREE	(1 << 0)

#define IN_CLEAN        0	/* in-block inode and memory copies identical */
#define IN_DIRTY        1	/* in-block inode and memory copies differ */
#define ATIME            002	/* set if atime field needs updating */
#define CTIME            004	/* set if ctime field needs updating */
#define MTIME            010	/* set if mtime field needs updating */

#define ROOT_INODE   ((ino_t) 1)	/* inode number for root directory */
#define BOOT_BLOCK  ((block_t) 0)	/* block number of boot block */
#define SUPER_BLOCK_BYTES  (1024)	/* bytes offset */
#define START_BLOCK ((block_t) 2)	/* first block of FS (not counting SB) */

#define DIR_ENTRY_SIZE       sizeof (struct direct)  /* # bytes/dir entry   */
#define NR_DIR_ENTRIES(b)   ((b)/DIR_ENTRY_SIZE)  /* # dir entries/blk   */
#define SUPER_SIZE      sizeof (struct super_block)  /* super_block size    */

#define FS_BITMAP_CHUNKS(b) ((b)/sizeof (bitchunk_t))/* # map chunks/blk   */
#define FS_BITCHUNK_BITS		(sizeof(bitchunk_t) * CHAR_BIT)
#define FS_BITS_PER_BLOCK(b)	(FS_BITMAP_CHUNKS(b) * FS_BITCHUNK_BITS)

/* Derived sizes pertaining to the V2 file system. */
#define V2_ZONE_NUM_SIZE            sizeof (zone_t)  /* # bytes in V2 zone  */
#define V2_INODE_SIZE             sizeof (d2_inode)  /* bytes in V2 dsk ino */
#define V2_INDIRECTS(b)   ((b)/V2_ZONE_NUM_SIZE)  /* # zones/indir block */
#define V2_INODES_PER_BLOCK(b) ((b)/V2_INODE_SIZE)/* # V2 dsk inodes/blk */

#endif

